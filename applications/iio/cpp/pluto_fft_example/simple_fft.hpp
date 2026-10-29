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
 * This file is based on operators/fft/fft.hpp from nvidia-holoscan/holohub,
 * with the VITA 49.2 metadata passthrough parameters removed (num_channels,
 * spectrum_type, averaging_type, window_time, window_type, transform_points,
 * window_points, resolution, span, weighting_factor, f1_index, f2_index,
 * window_time_delta) since nothing downstream in this example reads them.
 * This is the original license text:
 *
 * SPDX-FileCopyrightText: 2024-2026 Voyager Technologies, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <matx.h>

#include <cstdint>

#include "holoscan/holoscan.hpp"

using namespace matx;

using complex = cuda::std::complex<float>;

namespace holoscan::ops {

// Minimal FFT operator for pluto_fft_example. Computes a burst FFT via matx
// and shifts it so DC sits at the center bin (matching what
// FFTGnuplotOp/RealTimeFFTOp expect). Deliberately smaller than a
// general-purpose FFT operator: no VITA 49.2 metadata passthrough, since
// nothing downstream in this example reads it.
class SimpleFFT : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(SimpleFFT)

  SimpleFFT() = default;

  void initialize() override;
  void setup(OperatorSpec& spec) override;
  void compute(InputContext& input, OutputContext& output, ExecutionContext& context) override;

 private:
  Parameter<int> burst_size;
  Parameter<int> num_bursts;
};

}  // namespace holoscan::ops
