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

#pragma once

#include <holoscan/core/forward_def.hpp>
#include <holoscan/core/operator.hpp>
#include <holoscan/core/operator_spec.hpp>
#include <holoscan/holoscan.hpp>
#include <holoscan/logger/logger.hpp>

namespace holoscan::ops {

class MonopulseTracker : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(MonopulseTracker);

  MonopulseTracker() = default;
  ~MonopulseTracker() = default;

  void stop() override;
  void setup(OperatorSpec& spec) override;
  void initialize() override;
  void compute(InputContext& op_input, OutputContext& op_output, ExecutionContext& ec) override;

 private:
  // Parameters (set via YAML or constructor)
  Parameter<uint8_t> number_of_channels_p_;
  Parameter<uint32_t> number_of_samples_per_channel_p_;
  Parameter<ssize_t> sample_size_p_;
  Parameter<float> phase_step_p_;
  Parameter<float> current_phase_p_;
  Parameter<float> element_spacing_p_;
  Parameter<float> signal_frequency_p_;

  // Runtime state
  ssize_t sample_size_ = 8;
  uint8_t number_of_channels_ = 4;
  float current_phase_ = 0.0f;
  float element_spacing_ = 0.0f;
  float signal_frequency_ = 0.0f;
  uint32_t number_of_samples_per_channel_ = 16384;
  float phase_step_ = 5.0f;

  // GPU memory (allocated in initialize, freed in stop)
  void* d_correlation_terms_ = nullptr;
  void* d_input_data_ = nullptr;
};

}  // namespace holoscan::ops
