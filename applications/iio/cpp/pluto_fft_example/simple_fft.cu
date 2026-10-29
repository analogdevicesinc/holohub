/*
 * SPDX-FileCopyrightText: Copyright (c) 2026, Analog Devices, Inc. All rights reserved.
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

/*
 * This file is based on operators/fft/fft.cu from nvidia-holoscan/holohub,
 * with the VITA 49.2 metadata passthrough parameters removed (num_channels,
 * spectrum_type, averaging_type, window_time, window_type, transform_points,
 * window_points, resolution, span, weighting_factor, f1_index, f2_index,
 * window_time_delta) since nothing downstream in this example reads them.
 * This is the original license text:
 *
 * SPDX-FileCopyrightText: 2024-2026 Voyager Technologies, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "simple_fft.hpp"

#include <stdexcept>
#include <utility>

using in_t = std::tuple<tensor_t<complex, 2>, cudaStream_t>;
using out_t = std::tuple<tensor_t<complex, 2>, cudaStream_t>;

namespace holoscan::ops {

void SimpleFFT::setup(OperatorSpec& spec) {
  spec.input<in_t>("in");
  spec.output<out_t>("out");
  spec.param(burst_size, "burst_size", "Burst size", "Number of samples to process in each burst");
  spec.param(
      num_bursts, "num_bursts", "Number of bursts", "Number of sample bursts to process at once");
}

void SimpleFFT::initialize() {
  holoscan::Operator::initialize();
  if (burst_size.get() <= 0 || num_bursts.get() <= 0) {
    throw std::runtime_error("fft.burst_size and fft.num_bursts must both be > 0");
  }
}

void SimpleFFT::compute(InputContext& op_input, OutputContext& op_output, ExecutionContext&) {
  auto input = op_input.receive<in_t>("in").value();
  auto input_tensor = std::get<0>(input);
  auto stream = std::get<1>(input);

  if (input_tensor.Size(0) != num_bursts.get() || input_tensor.Size(1) != burst_size.get()) {
    throw std::runtime_error(fmt::format(
        "FFT input shape ({}, {}) must match configured shape ({}, {})",
        input_tensor.Size(0),
        input_tensor.Size(1),
        num_bursts.get(),
        burst_size.get()));
  }

  // Every emitted message owns its output storage. Reusing one tensor per
  // channel can overwrite data while another branch still consumes it.
  auto out = make_tensor<complex>(
      {static_cast<index_t>(num_bursts.get()), static_cast<index_t>(burst_size.get())},
      MATX_ASYNC_DEVICE_MEMORY,
      stream);

  (out = fftshift1D(fft(input_tensor))).run(stream);

  op_output.emit(out_t{std::move(out), stream}, "out");
}

}  // namespace holoscan::ops
