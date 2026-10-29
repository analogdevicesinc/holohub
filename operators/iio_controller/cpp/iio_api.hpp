/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026, Analog Devices, Inc. All rights reserved.
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

#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct iio_context;
struct iio_device;
struct iio_channel;

namespace holoscan::ops {

// Mirrors libiio's iio_data_format. v1 adds `offset`; v0 reports 0 for it.
struct IIODataFormat {
  unsigned int length = 0;
  unsigned int bits = 0;
  unsigned int shift = 0;
  bool is_signed = false;
  bool is_fully_defined = false;
  bool is_be = false;
  bool with_scale = false;
  double scale = 0.0;
  unsigned int repeat = 0;
  double offset = 0.0;
};

enum class IIODeviceAttrType { DEVICE, DEBUG, BUFFER };

// Abstracts the libiio calls used by the iio_controller operators so they
// build and run against both libiio v0 and v1. Exactly one of IIOApiV0 /
// IIOApiV1 is compiled in, selected by CMake based on the detected libiio
// version; make_iio_api() is defined once, in whichever backend got built.
class IIOApi {
 public:
  virtual ~IIOApi() = default;

  virtual iio_context* create_context(const std::string& uri) = 0;
  virtual void destroy_context(iio_context* ctx) = 0;
  virtual iio_device* find_device(iio_context* ctx, const std::string& name) = 0;
  virtual iio_channel* find_channel(iio_device* dev, const std::string& name, bool is_output) = 0;

  virtual std::string device_name(const iio_device* dev) = 0;
  virtual std::string channel_id(const iio_channel* chn) = 0;
  virtual bool channel_is_output(const iio_channel* chn) = 0;
  virtual int64_t channel_index(const iio_channel* chn) = 0;
  virtual IIODataFormat channel_format(const iio_channel* chn) = 0;
  virtual void convert(const iio_channel* chn, void* dst, const void* src) = 0;
  virtual void convert_inverse(const iio_channel* chn, void* dst, const void* src) = 0;

  virtual void enable_channel(iio_channel* chn) = 0;
  virtual bool is_channel_enabled(const iio_channel* chn) = 0;
  virtual ssize_t sample_size(iio_device* dev) = 0;

  virtual bool open_buffer(iio_device* dev, size_t samples_count, bool cyclic) = 0;
  virtual ssize_t refill(void** data) = 0;
  virtual ssize_t push(const void* data, size_t bytes) = 0;
  virtual void close_buffer() = 0;

  virtual ssize_t read_attr(iio_context* ctx, const std::string& name, std::string& out) = 0;
  virtual ssize_t read_attr(iio_device* dev, IIODeviceAttrType type, const std::string& name,
                            std::string& out) = 0;
  virtual ssize_t read_attr(iio_channel* chn, const std::string& name, std::string& out) = 0;
  virtual ssize_t write_attr(iio_device* dev, IIODeviceAttrType type, const std::string& name,
                             const std::string& value) = 0;
  virtual ssize_t write_attr(iio_channel* chn, const std::string& name,
                             const std::string& value) = 0;
};

std::unique_ptr<IIOApi> make_iio_api();

}  // namespace holoscan::ops
