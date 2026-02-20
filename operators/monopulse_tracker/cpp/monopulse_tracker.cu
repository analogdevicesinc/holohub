/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Analog Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "monopulse_tracker.hpp"

#include <cuda_runtime.h>
#include <thrust/complex.h>
#include <thrust/device_vector.h>
#include <thrust/reduce.h>
#include <thrust/transform.h>
#include <dlpack/dlpack.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <memory>

namespace {

// CUDA kernel: Compute sum beam, delta beam, and correlation term in one pass
__global__ void monopulse_kernel(
    const thrust::complex<float>* __restrict__ ch0,
    const thrust::complex<float>* __restrict__ ch1,
    const thrust::complex<float>* __restrict__ ch2,
    const thrust::complex<float>* __restrict__ ch3,
    thrust::complex<float>* __restrict__ correlation_terms,
    int num_samples) {

  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= num_samples) return;

  // Sum beam: ch0 + ch1 + ch2 + ch3
  thrust::complex<float> sum_val = ch0[idx] + ch1[idx] + ch2[idx] + ch3[idx];

  // Delta beam: (ch0 + ch1) - (ch2 + ch3)
  thrust::complex<float> delta_val = (ch0[idx] + ch1[idx]) - (ch2[idx] + ch3[idx]);

  // Correlation term: sum * conj(delta)
  correlation_terms[idx] = sum_val * thrust::conj(delta_val);
}

// Helper to create a 1-element float tensor (hides DLPack boilerplate)
std::shared_ptr<holoscan::Tensor> make_scalar_tensor(float value) {
  auto data = new float[1]{value};
  auto dl = new DLManagedTensor();
  dl->dl_tensor = {data, {kDLCPU, 0}, 1, {kDLFloat, 32, 1}, new int64_t[1]{1}, nullptr, 0};
  dl->manager_ctx = data;
  dl->deleter = [](DLManagedTensor* self) {
    delete[] static_cast<float*>(self->manager_ctx);
    delete[] self->dl_tensor.shape;
    delete self;
  };
  return std::make_shared<holoscan::Tensor>(dl);
}

}  // namespace

using namespace holoscan::ops;

void MonopulseTracker::stop() {
  // Free GPU memory if allocated
  if (d_correlation_terms_) {
    cudaFree(d_correlation_terms_);
    d_correlation_terms_ = nullptr;
  }
  if (d_input_data_) {
    cudaFree(d_input_data_);
    d_input_data_ = nullptr;
  }
}

void MonopulseTracker::setup(OperatorSpec& spec) {
  // Input: phase-adjusted complex IQ data (receive as GXF Entity)
  spec.input<holoscan::gxf::Entity>("phase_adjusted_channels");

  // Outputs
  spec.output<holoscan::gxf::Entity>("steer_angle");    // Steering angle for visualization
  spec.output<holoscan::gxf::Entity>("current_phase");  // Current phase for feedback loop

  // Parameter specifications
  spec.param(sample_size_p_, "sample_size", "Sample Size",
             "Size of each sample in bytes (complex<float> = 8)",
             static_cast<ssize_t>(8));

  spec.param(number_of_channels_p_, "number_of_channels", "Number of Channels",
             "Number of RX channels",
             static_cast<uint8_t>(4));

  spec.param(number_of_samples_per_channel_p_, "number_of_samples_per_channel",
             "Samples Per Channel",
             "Number of samples per channel",
             static_cast<uint32_t>(16384));

  spec.param(element_spacing_p_, "element_spacing", "Element Spacing",
             "Antenna element spacing in meters");

  spec.param(signal_frequency_p_, "signal_frequency", "Signal Frequency",
             "Signal frequency in Hz");

  spec.param(current_phase_p_, "current_phase", "Current Phase",
             "Initial phase in degrees",
             0.0f);

  spec.param(phase_step_p_, "phase_step", "Phase Step",
             "Phase adjustment step in degrees",
             5.0f);
}

void MonopulseTracker::initialize() {
  Operator::initialize();

  sample_size_ = sample_size_p_.get();
  if (sample_size_ <= 0) {
    HOLOSCAN_LOG_ERROR("Invalid sample size: {}. Sample size must be greater than 0.", sample_size_);
    return;
  }

  number_of_channels_ = number_of_channels_p_.get();
  if (number_of_channels_ == 0) {
    HOLOSCAN_LOG_ERROR("Invalid number of channels: {}. Number of channels must be greater than 0.", number_of_channels_);
    return;
  }

  number_of_samples_per_channel_ = number_of_samples_per_channel_p_.get();
  if (number_of_samples_per_channel_ == 0) {
    HOLOSCAN_LOG_ERROR("Invalid number of samples per channel: {}. Number of samples per channel must be greater than 0.", number_of_samples_per_channel_);
    return;
  }

  current_phase_ = current_phase_p_.get();
  if (current_phase_ < 0.0f || current_phase_ >= 360.0f) {
    HOLOSCAN_LOG_ERROR("Invalid current phase: {}. Current phase must be in the range [0, 360).", current_phase_);
    return;
  }

  element_spacing_ = element_spacing_p_.get();
  if (element_spacing_ <= 0.0f) {
    HOLOSCAN_LOG_ERROR("Invalid element spacing: {}. Element spacing must be greater than 0.", element_spacing_);
    return;
  }

  signal_frequency_ = signal_frequency_p_.get();
  if (signal_frequency_ <= 0.0f) {
    HOLOSCAN_LOG_ERROR("Invalid signal frequency: {}. Signal frequency must be greater than 0.", signal_frequency_);
    return;
  }

  phase_step_ = phase_step_p_.get();
  if (phase_step_ <= 0.0f) {
    HOLOSCAN_LOG_ERROR("Invalid phase step: {}. Phase step must be greater than 0.", phase_step_);
    return;
  }

  // Pre-allocate GPU memory for expected buffer size
  size_t num_samples = number_of_samples_per_channel_;
  cudaMalloc(&d_correlation_terms_, num_samples * sizeof(thrust::complex<float>));
  cudaMalloc(&d_input_data_, 4 * num_samples * sizeof(thrust::complex<float>));

  HOLOSCAN_LOG_INFO("MonopulseTracker CUDA initialized with {} samples per channel", num_samples);
}

void MonopulseTracker::compute(InputContext& op_input, OutputContext& op_output, ExecutionContext& context) {
  // Receive as GXF Entity
  auto maybe_entity = op_input.receive<holoscan::gxf::Entity>("phase_adjusted_channels");

  if (!maybe_entity) {
    HOLOSCAN_LOG_ERROR("Failed to receive phase_adjusted_channels - no data available");
    return;
  }

  auto entity = maybe_entity.value();

  // Try to get tensor from entity with empty name (default tensor)
  auto tensor_ptr = entity.get<holoscan::Tensor>("");
  if (!tensor_ptr) {
    HOLOSCAN_LOG_ERROR("Failed to extract tensor from entity");
    return;
  }

  // Get shape
  auto shape = tensor_ptr->shape();
  if (shape.size() != 2 || shape[0] != 4) {
    HOLOSCAN_LOG_ERROR("Invalid tensor shape: expected [4, num_samples], got {} dimensions", shape.size());
    return;
  }

  int num_samples = shape[1];

  // Reallocate GPU memory if buffer size changed
  if (static_cast<uint32_t>(num_samples) != number_of_samples_per_channel_) {
    number_of_samples_per_channel_ = num_samples;
    cudaFree(d_correlation_terms_);
    cudaFree(d_input_data_);
    cudaMalloc(&d_correlation_terms_, num_samples * sizeof(thrust::complex<float>));
    cudaMalloc(&d_input_data_, 4 * num_samples * sizeof(thrust::complex<float>));
  }

  // Get data pointer (on CPU)
  auto* h_data_ptr = reinterpret_cast<thrust::complex<float>*>(tensor_ptr->data());

  // Copy input data to GPU
  cudaMemcpy(d_input_data_, h_data_ptr, 4 * num_samples * sizeof(thrust::complex<float>), cudaMemcpyHostToDevice);

  // Channel pointers on GPU (cast from void*)
  auto* d_data = static_cast<thrust::complex<float>*>(d_input_data_);
  thrust::complex<float>* d_ch0 = d_data;
  thrust::complex<float>* d_ch1 = d_data + num_samples;
  thrust::complex<float>* d_ch2 = d_data + 2 * num_samples;
  thrust::complex<float>* d_ch3 = d_data + 3 * num_samples;

  // Launch kernel to compute correlation terms
  int threads_per_block = 256;
  int num_blocks = (num_samples + threads_per_block - 1) / threads_per_block;

  auto* d_corr = static_cast<thrust::complex<float>*>(d_correlation_terms_);

  monopulse_kernel<<<num_blocks, threads_per_block>>>(
      d_ch0, d_ch1, d_ch2, d_ch3,
      d_corr,
      num_samples);

  // Use thrust to reduce correlation terms (sum all elements)
  thrust::device_ptr<thrust::complex<float>> d_ptr(d_corr);
  thrust::complex<float> correlation = thrust::reduce(
      d_ptr, d_ptr + num_samples,
      thrust::complex<float>(0.0f, 0.0f),
      thrust::plus<thrust::complex<float>>());

  // Extract phase error (on CPU)
  float error = atan2f(correlation.imag(), correlation.real());

  // Update phase
  if (error > 0) {
    current_phase_ -= phase_step_;
  } else {
    current_phase_ += phase_step_;
  }

  // Calculate steer angle
  float phase_rad = current_phase_ * M_PI / 180.0f;
  float sin_term = (3e8f * phase_rad) / (2.0f * M_PI * signal_frequency_ * element_spacing_);
  sin_term = std::clamp(sin_term, -1.0f, 1.0f);
  float steer_angle = std::asin(sin_term) * 180.0f / M_PI;

  // Emit steer_angle for visualization
  auto steer_message = holoscan::gxf::Entity::New(&context);
  steer_message.add(make_scalar_tensor(steer_angle), "");
  op_output.emit(steer_message, "steer_angle");

  // Emit current_phase for feedback loop
  auto phase_message = holoscan::gxf::Entity::New(&context);
  phase_message.add(make_scalar_tensor(current_phase_), "");
  op_output.emit(phase_message, "current_phase");
}
