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

#include <string>

namespace holoscan::doc::MonopulseTracker {

constexpr const char* doc_MonopulseTracker_python = R"doc(
Monopulse tracking operator for phased array processing.

Calculates sum and delta beams from phase-adjusted IQ data,
computes phase error via correlation, and updates the tracking phase
to estimate angle of arrival.

**Named Inputs**

phase_adjusted_channels : holoscan.Tensor
    Complex IQ data with shape [4, num_samples] representing 4 receive channels
    with current phase shift already applied. Data type: complex64 (std::complex<float>)

**Named Outputs**

steer_angle : holoscan.Tensor
    Estimated angle of arrival in degrees (1-element float tensor)
current_phase : holoscan.Tensor
    Updated tracking phase in degrees (1-element float tensor) for feedback loop

Parameters
----------
fragment : holoscan.Fragment
    The fragment that the operator belongs to.
element_spacing : float
    Antenna element spacing in meters (required)
signal_frequency : float
    Signal frequency in Hz (required)
phase_step : float, optional
    Phase adjustment step size in degrees (default: 5.0)
current_phase : float, optional
    Initial phase in degrees (default: 0.0)
number_of_channels : int, optional
    Number of receive channels (default: 4)
number_of_samples_per_channel : int, optional
    Number of samples per channel (default: 16384)
sample_size : int, optional
    Size of each sample in bytes (default: 8 for complex<float>)
name : str, optional
    The name of the operator (default: "monopulse_tracker")
)doc";

constexpr const char* doc_initialize = R"doc(
Initialize the operator.

This method is called when the operator is created and validates all parameters.
)doc";

}  // namespace holoscan::doc::MonopulseTracker
