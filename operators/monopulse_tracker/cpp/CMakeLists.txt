# SPDX-FileCopyrightText: Copyright (c) 2025 Analog Devices, Inc. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.20)
project(monopulse_tracker LANGUAGES CXX CUDA)

find_package(
  holoscan
  2.6
  REQUIRED
  CONFIG
  PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan-sdk/install")

find_package(CUDAToolkit REQUIRED)

add_library(
  monopulse_tracker SHARED
  monopulse_tracker.cu)

add_library(holoscan::ops::monopulse_tracker ALIAS monopulse_tracker)

set_target_properties(monopulse_tracker PROPERTIES
  CUDA_SEPARABLE_COMPILATION ON
  CUDA_ARCHITECTURES "72;87"  # Jetson Xavier (72), Orin (87)
)

target_link_libraries(
  monopulse_tracker
  PRIVATE holoscan::core
          CUDA::cudart)

target_include_directories(
  monopulse_tracker
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         $<INSTALL_INTERFACE:include>
         $<INSTALL_INTERFACE:include/holoscan/operators/monopulse_tracker>)

set_target_properties(
  monopulse_tracker PROPERTIES OUTPUT_NAME "holoscan_op_monopulse_tracker"
                            EXPORT_NAME ops::monopulse_tracker)

install(
  TARGETS monopulse_tracker
  EXPORT holoscan-monopulse-tracker-targets
  COMPONENT monopulse_tracker-cpp)

install(
  FILES monopulse_tracker.hpp
  DESTINATION include/holoscan/operators/monopulse_tracker
  COMPONENT monopulse_tracker-cpp)
