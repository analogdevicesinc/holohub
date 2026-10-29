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

#include <iio.h>

#include <cerrno>
#include <cstring>

#include "iio_api.hpp"

namespace holoscan::ops {

namespace {

ssize_t device_attr_read(iio_device* dev, IIODeviceAttrType type, const char* name, char* dst,
                         size_t len) {
  switch (type) {
    case IIODeviceAttrType::DEVICE:
      return iio_device_attr_read(dev, name, dst, len);
    case IIODeviceAttrType::DEBUG:
      return iio_device_debug_attr_read(dev, name, dst, len);
    case IIODeviceAttrType::BUFFER:
      return iio_device_buffer_attr_read(dev, name, dst, len);
  }
  return -EINVAL;
}

ssize_t device_attr_write(iio_device* dev, IIODeviceAttrType type, const char* name,
                          const char* src) {
  switch (type) {
    case IIODeviceAttrType::DEVICE:
      return iio_device_attr_write(dev, name, src);
    case IIODeviceAttrType::DEBUG:
      return iio_device_debug_attr_write(dev, name, src);
    case IIODeviceAttrType::BUFFER:
      return iio_device_buffer_attr_write(dev, name, src);
  }
  return -EINVAL;
}

}  // namespace

class IIOApiV0 : public IIOApi {
 public:
  ~IIOApiV0() override { close_buffer(); }

  iio_context* create_context(const std::string& uri) override {
    return iio_create_context_from_uri(uri.c_str());
  }

  void destroy_context(iio_context* ctx) override { iio_context_destroy(ctx); }

  iio_device* find_device(iio_context* ctx, const std::string& name) override {
    return iio_context_find_device(ctx, name.c_str());
  }

  iio_channel* find_channel(iio_device* dev, const std::string& name, bool is_output) override {
    return iio_device_find_channel(dev, name.c_str(), is_output);
  }

  std::string device_name(const iio_device* dev) override {
    const char* name = iio_device_get_name(dev);
    return name ? name : "";
  }

  std::string channel_id(const iio_channel* chn) override {
    const char* id = iio_channel_get_id(chn);
    return id ? id : "";
  }

  bool channel_is_output(const iio_channel* chn) override { return iio_channel_is_output(chn); }

  int64_t channel_index(const iio_channel* chn) override { return iio_channel_get_index(chn); }

  IIODataFormat channel_format(const iio_channel* chn) override {
    const iio_data_format* fmt = iio_channel_get_data_format(chn);
    IIODataFormat out;
    if (fmt) {
      out.length = fmt->length;
      out.bits = fmt->bits;
      out.shift = fmt->shift;
      out.is_signed = fmt->is_signed;
      out.is_fully_defined = fmt->is_fully_defined;
      out.is_be = fmt->is_be;
      out.with_scale = fmt->with_scale;
      out.scale = fmt->scale;
      out.repeat = fmt->repeat;
      out.offset = 0.0;
    }
    return out;
  }

  void convert(const iio_channel* chn, void* dst, const void* src) override {
    iio_channel_convert(chn, dst, src);
  }

  void convert_inverse(const iio_channel* chn, void* dst, const void* src) override {
    iio_channel_convert_inverse(chn, dst, src);
  }

  void enable_channel(iio_channel* chn) override { iio_channel_enable(chn); }

  bool is_channel_enabled(const iio_channel* chn) override { return iio_channel_is_enabled(chn); }

  ssize_t sample_size(iio_device* dev) override { return iio_device_get_sample_size(dev); }

  bool open_buffer(iio_device* dev, size_t samples_count, bool cyclic) override {
    close_buffer();
    buffer_ = iio_device_create_buffer(dev, samples_count, cyclic);
    return buffer_ != nullptr;
  }

  ssize_t refill(void** data) override {
    ssize_t ret = iio_buffer_refill(buffer_);
    if (ret < 0) return ret;
    *data = iio_buffer_start(buffer_);
    return ret;
  }

  ssize_t push(const void* data, size_t bytes) override {
    void* dst = iio_buffer_start(buffer_);
    memcpy(dst, data, bytes);
    return iio_buffer_push(buffer_);
  }

  void close_buffer() override {
    if (buffer_) {
      iio_buffer_destroy(buffer_);
      buffer_ = nullptr;
    }
  }

  ssize_t read_attr(iio_context* ctx, const std::string& name, std::string& out) override {
    const char* value = iio_context_get_attr_value(ctx, name.c_str());
    if (!value) return -ENOENT;
    out = value;
    return static_cast<ssize_t>(out.size());
  }

  ssize_t read_attr(iio_device* dev, IIODeviceAttrType type, const std::string& name,
                    std::string& out) override {
    char buf[1024];
    ssize_t ret = device_attr_read(dev, type, name.c_str(), buf, sizeof(buf));
    if (ret >= 0) out = buf;
    return ret;
  }

  ssize_t read_attr(iio_channel* chn, const std::string& name, std::string& out) override {
    char buf[1024];
    ssize_t ret = iio_channel_attr_read(chn, name.c_str(), buf, sizeof(buf));
    if (ret >= 0) out = buf;
    return ret;
  }

  ssize_t write_attr(iio_device* dev, IIODeviceAttrType type, const std::string& name,
                     const std::string& value) override {
    return device_attr_write(dev, type, name.c_str(), value.c_str());
  }

  ssize_t write_attr(iio_channel* chn, const std::string& name, const std::string& value) override {
    return iio_channel_attr_write(chn, name.c_str(), value.c_str());
  }

 private:
  iio_buffer* buffer_ = nullptr;
};

std::unique_ptr<IIOApi> make_iio_api() { return std::make_unique<IIOApiV0>(); }

}  // namespace holoscan::ops
