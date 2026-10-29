# SPDX-FileCopyrightText: Copyright (c) 2026 Your Name / Your Organization. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 4.0)

project(holoscan_iio_controller
    VERSION 0.1.0
    DESCRIPTION "A Holoscan Module extending the Holoscan SDK."
    LANGUAGES CXX
)

# Holoscan SDK 5.0 headers require C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(holoscan 4.6 REQUIRED CONFIG COMPONENTS core)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
# BUILD_ALL toggles every operator/application/package on by default. It
# matches the HoloHub-monorepo convention: each subproject is a CMake option
# (e.g. APP_<name>, OP_<name>, PKG_<name>) defined via add_holohub_application
# / add_holohub_operator / add_holohub_package, defaulting to ${BUILD_ALL}.
# To build a single subproject, configure with -DBUILD_ALL=OFF -DAPP_<name>=ON.
#
# Default to ON only when this is the top-level CMake project (standalone
# build). When nested under another project (e.g. consumed as a HoloHub
# external dependency via add_subdirectory), defer to whatever the parent
# build has decided — otherwise this module's BUILD_ALL=ON would leak into
# the parent's variable scope and force-enable unrelated subprojects there.

if(NOT DEFINED BUILD_ALL)
    if(PROJECT_IS_TOP_LEVEL)
        set(BUILD_ALL ON CACHE BOOL "Build all operators, applications, and packages by default")
    else()
        set(BUILD_ALL OFF)
    endif()
endif()

# The Python bindings follow the same rule: ON when standalone, OFF when nested.
if(NOT DEFINED HOLOSCAN_IIO_CONTROLLER_BUILD_PYTHON)
    if(PROJECT_IS_TOP_LEVEL)
        set(HOLOSCAN_IIO_CONTROLLER_BUILD_PYTHON ON
            CACHE BOOL "Build the holoscan_iio_controller Python bindings")
    else()
        set(HOLOSCAN_IIO_CONTROLLER_BUILD_PYTHON OFF
            CACHE BOOL "Build the holoscan_iio_controller Python bindings")
    endif()
endif()
if(HOLOSCAN_IIO_CONTROLLER_BUILD_PYTHON)
    # The operator libraries are linked into the Python extension module.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    # Found at the root so python/ and tests/python/ see the same interpreter.
    if(Python3_EXECUTABLE AND NOT Python_EXECUTABLE)
        set(Python_EXECUTABLE "${Python3_EXECUTABLE}")
    endif()
    find_package(Python 3.12 REQUIRED
        COMPONENTS Interpreter Development.Module Development.SABIModule)
endif()

# Application languages. The Holoscan CLI sets these from --language or the
# application's metadata.json; a plain CMake build includes every language.
option(HOLOSCAN_IIO_CONTROLLER_BUILD_CPP "Build C++ applications" ON)

# ---------------------------------------------------------------------------
# CMake helpers
# ---------------------------------------------------------------------------
# HoloHubConfigHelpers.cmake provides add_holohub_application/operator/package
# which gate each subproject on its own option (APP_/OP_/PKG_<name>). The
# helpers are bundled directly in this generated repository so it builds
# without a HoloHub clone.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HoloHubConfigHelpers)

add_library(holoscan_iio_controller INTERFACE)
add_library(holoscan::iio_controller ALIAS holoscan_iio_controller)
set_target_properties(holoscan_iio_controller PROPERTIES
    EXPORT_NAME iio_controller)
install(TARGETS holoscan_iio_controller
    EXPORT holoscan_iio_controller_targets)

# Every install and package carries the license and third-party notices.
include(GNUInstallDirs)
install(FILES LICENSE NOTICE
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/holoscan-iio-controller")

# ---------------------------------------------------------------------------
# Subprojects
# ---------------------------------------------------------------------------
add_subdirectory(operators)
add_subdirectory(applications)
