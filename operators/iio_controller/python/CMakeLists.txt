# SPDX-FileCopyrightText: Copyright (c) 2025-2026, Analog Devices, Inc. All rights reserved.
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

find_package(
  holoscan
  2.6
  REQUIRED
  CONFIG
  PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan-sdk/install")

include(pybind11_add_holohub_module)
pybind11_add_holohub_module(
  CPP_CMAKE_TARGET
  iio_controller
  PYTHON_NAMESPACE
  holoscan
  PYTHON_MODULE_NAME
  iio_controller
  CLASS_NAME
  "IIOAttributeRead, IIOAttributeWrite, IIOBufferInfo, IIOBufferRead, IIOBufferWrite, IIOConfigurator, IIOChannelInfo, IIODataFormat"
  SOURCES
  iio_controller_py.cpp)

if(BUILD_TESTING)
  set(PYTHON_TEST_ENV
      "PYTHONPATH=${CMAKE_BINARY_DIR}/python/lib:$ENV{PYTHONPATH}"
  )

  add_test(
    NAME iio_controller_python_import_test
    COMMAND python3 -c
            "from holoscan.iio_controller import IIOAttributeRead; assert IIOAttributeRead.__name__ == 'IIOAttributeRead'"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )

  add_test(
    NAME iio_controller_python_pytest
    COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

  set_tests_properties(
    iio_controller_python_import_test
    iio_controller_python_pytest
    PROPERTIES
      ENVIRONMENT "${PYTHON_TEST_ENV}"
  )
endif()
