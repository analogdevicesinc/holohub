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
project(iio_controller)

find_package(
  holoscan
  2.6
  REQUIRED
  CONFIG
  PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan-sdk/install")

# Detect the installed libiio major version (v0 vs v1), since their APIs are
# incompatible. Prefer pkg-config, since it pairs a header with the matching
# library; fall back to a header-path probe if no .pc file is installed.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(LIBIIO QUIET IMPORTED_TARGET libiio)
endif()

if(LIBIIO_FOUND AND LIBIIO_VERSION)
  if(LIBIIO_VERSION VERSION_LESS 1.0)
    set(LIBIIO_V1 FALSE)
  else()
    set(LIBIIO_V1 TRUE)
  endif()
  set(IIO_LIBRARIES PkgConfig::LIBIIO)
  message(STATUS "libiio ${LIBIIO_VERSION} found via pkg-config")
else()
  message(STATUS "pkg-config found no libiio .pc file, falling back to header probe")
  find_path(IIO_V1_INCLUDE_DIR NAMES iio/iio.h)
  if(IIO_V1_INCLUDE_DIR)
    set(LIBIIO_V1 TRUE)
  else()
    set(LIBIIO_V1 FALSE)
  endif()

  find_library(
    IIO_LIBRARIES
    NAMES iio libiio
    HINTS /usr/lib
          /usr/lib64
          /usr/local/lib
          /usr/local/lib64
          /opt/lib
          /opt/lib64
          REQUIRED)
  message(STATUS "libiio found at ${IIO_LIBRARIES} (v1: ${LIBIIO_V1})")
endif()

if(LIBIIO_V1)
  message(FATAL_ERROR "libiio v1 backend not implemented yet")
endif()

add_library(
  iio_controller SHARED
  iio_api.hpp
  iio_api_v0.cpp
  iio_params.hpp
  iio_attribute_read.hpp
  iio_attribute_read.cpp
  iio_attribute_write.hpp
  iio_attribute_write.cpp
  iio_buffer_read.hpp
  iio_buffer_read.cpp
  iio_buffer_write.hpp
  iio_buffer_write.cpp
  iio_configurator.hpp
  iio_configurator.cpp)

add_library(holoscan::ops::iio_controller ALIAS iio_controller)

target_link_libraries(
  iio_controller
  PUBLIC ${IIO_LIBRARIES}
  PRIVATE holoscan::core)

target_include_directories(
  iio_controller
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         $<INSTALL_INTERFACE:include>
         $<INSTALL_INTERFACE:include/holoscan/operators/iio_controller>)

set_target_properties(
  iio_controller PROPERTIES OUTPUT_NAME "holoscan_op_iio_controller"
                            EXPORT_NAME ops::iio_controller)

install(
  TARGETS iio_controller
  EXPORT holoscan-iio-targets
  COMPONENT iio_controller-cpp)

install(
  FILES iio_controller.hpp iio_params.hpp iio_api.hpp
  DESTINATION include/holoscan/operators/iio_controller
  COMPONENT iio_controller-cpp)
