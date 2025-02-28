# SPDX-FileCopyrightText: Copyright (c) 2025 ADI Inc.
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
project(network_radar_pipeline CXX CUDA)

find_package(holoscan 2.0 REQUIRED CONFIG
             PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install")

# set(CMAKE_BUILD_TYPE Debug)
# Enable cuda language
set(CMAKE_CUDA_ARCHITECTURES "70;80;90")
enable_language(CUDA)

set(REL_PTH_MATLAB_CODEGEN "matlab/codegen")  # Needs changing if building on Jetson

set(EXECUTABLES matlab_classify_modulator)

add_executable(matlab_classify_modulator_adv
  main.cpp
)

# set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -ggdb")
# set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -G")

add_library(matlab_classyfy_modulation_lib SHARED IMPORTED)
set_target_properties(matlab_classyfy_modulation_lib
  PROPERTIES IMPORTED_LOCATION
  "${CMAKE_CURRENT_SOURCE_DIR}/${REL_PTH_MATLAB_CODEGEN}/dll/classifyModulation/classifyModulation.so"
)

target_include_directories(matlab_classify_modulator_adv PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/${REL_PTH_MATLAB_CODEGEN}/dll/classifyModulation"
  "$ENV{MATLAB_ROOT}/extern/include"
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../operators/advanced_network
)

target_link_libraries(matlab_classify_modulator_adv PRIVATE
  holoscan::core
  holoscan::advanced_network_rx
  matlab_classyfy_modulation_lib
)

set_target_properties(matlab_classify_modulator_adv PROPERTIES
  CUDA_ARCHITECTURES "80;90"
)

target_include_directories(matlab_classify_modulator_adv PRIVATE
    ${PROJECT_SOURCE_DIR}/matlab_utils
)

add_custom_target("matlab_classify_modulator_adv_deps"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/matlab_classify_modulator_adv.yaml" ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS "matlab_classify_modulator_adv.yaml"
  BYPRODUCTS "matlab_classify_modulator_adv.yaml"
)
add_dependencies(matlab_classify_modulator_adv "matlab_classify_modulator_adv_deps")
