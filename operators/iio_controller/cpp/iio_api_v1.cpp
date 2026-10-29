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

#include "iio_api.hpp"

#include <iio/iio.h>

#include <cerrno>
#include <cstring>

namespace holoscan::ops {

namespace {

// Many libiio v1 pointer-returning functions signal failure with an
// error-encoded pointer rather than NULL (see iio_err() in <iio/iio.h>);
// some still just return NULL. This covers both conventions.
bool iio_failed(const void* ptr) { return !ptr || iio_err(ptr); }

const iio_attr* find_device_attr(iio_device* dev, IIODeviceAttrType type, const char* name) {
  switch (type) {
    case IIODeviceAttrType::DEVICE:
      return iio_device_find_attr(dev, name);
    case IIODeviceAttrType::DEBUG:
      return iio_device_find_debug_attr(dev, name);
    case IIODeviceAttrType::BUFFER: {
      iio_buffer* buf = iio_device_get_buffer(dev, 0);
      if (iio_failed(buf)) return nullptr;
      return iio_buffer_find_attr(buf, name);
    }
  }
  return nullptr;
}

}  // namespace

class IIOApiV1 : public IIOApi {
 public:
  ~IIOApiV1() override {
    close_buffer();
    if (mask_) iio_channels_mask_destroy(mask_);
  }

  iio_context* create_context(const std::string& uri) override {
    iio_context* ctx = iio_create_context(nullptr, uri.c_str());
    return iio_failed(ctx) ? nullptr : ctx;
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

  long channel_index(const iio_channel* chn) override { return iio_channel_get_index(chn); }

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
      out.offset = fmt->offset;
    }
    return out;
  }

  void convert(const iio_channel* chn, void* dst, const void* src) override {
    iio_channel_convert(chn, dst, src);
  }

  void convert_inverse(const iio_channel* chn, void* dst, const void* src) override {
    iio_channel_convert_inverse(chn, dst, src);
  }

  void enable_channel(iio_channel* chn) override {
    if (!ensure_mask(iio_channel_get_device(chn))) return;
    iio_channel_enable(chn, mask_);
  }

  bool is_channel_enabled(const iio_channel* chn) override {
    if (!ensure_mask(iio_channel_get_device(chn))) return false;
    return iio_channel_is_enabled(chn, mask_);
  }

  ssize_t sample_size(iio_device* dev) override {
    if (!ensure_mask(dev)) return -ENOMEM;
    return iio_device_get_sample_size(dev, mask_);
  }

  bool open_buffer(iio_device* dev, size_t samples_count, bool cyclic) override {
    close_buffer();
    if (!ensure_mask(dev)) return false;

    iio_buffer* buf = iio_device_get_buffer(dev, 0);
    if (iio_failed(buf)) return false;

    stream_ = iio_buffer_open(buf, mask_);
    if (iio_failed(stream_)) {
      stream_ = nullptr;
      return false;
    }

    ssize_t sample_bytes = iio_device_get_sample_size(dev, mask_);
    if (sample_bytes < 0) {
      close_buffer();
      return false;
    }

    block_size_ = samples_count * static_cast<size_t>(sample_bytes);
    block_ = iio_buffer_stream_create_block(stream_, block_size_);
    if (iio_failed(block_)) {
      block_ = nullptr;
      close_buffer();
      return false;
    }

    cyclic_ = cyclic;
    started_ = false;
    return true;
  }

  // RX: enqueue the block (arming it to be filled), start the stream on the
  // first call, then dequeue (blocking until filled) and hand back a pointer
  // to the data. The caller must copy the data out before the next refill()
  // call, since that call re-enqueues the same block.
  ssize_t refill(void** data) override {
    int ret = iio_block_enqueue(block_, 0, false);
    if (ret < 0) return ret;

    if (!started_) {
      ret = iio_buffer_stream_start(stream_);
      if (ret < 0) return ret;
      started_ = true;
    }

    ret = iio_block_dequeue(block_, false);
    if (ret < 0) return ret;

    *data = iio_block_start(block_);
    return static_cast<ssize_t>(block_size_);
  }

  // TX: dequeue the previous block (if any) to reclaim it, copy the new data
  // in, then enqueue it again, starting the stream on the first call.
  //
  // A cyclic buffer that is already enqueued and streaming must reject any
  // further push().
  ssize_t push(const void* data, size_t bytes) override {
    if (started_ && cyclic_) return -EBUSY;

    if (started_) {
      int ret = iio_block_dequeue(block_, false);
      if (ret < 0) return ret;
    }

    memcpy(iio_block_start(block_), data, bytes);

    int ret = iio_block_enqueue(block_, bytes, cyclic_);
    if (ret < 0) return ret;

    if (!started_) {
      ret = iio_buffer_stream_start(stream_);
      if (ret < 0) return ret;
      started_ = true;
    }

    return static_cast<ssize_t>(bytes);
  }

  void close_buffer() override {
    if (block_) {
      iio_block_destroy(block_);
      block_ = nullptr;
    }
    if (stream_) {
      iio_buffer_close(stream_);
      stream_ = nullptr;
    }
    started_ = false;
  }

  ssize_t read_attr(iio_context* ctx, const std::string& name, std::string& out) override {
    const iio_attr* attr = iio_context_find_attr(ctx, name.c_str());
    if (!attr) return -ENOENT;
    const char* value = iio_attr_get_static_value(attr);
    if (!value) return -ENOENT;
    out = value;
    return static_cast<ssize_t>(out.size());
  }

  ssize_t read_attr(iio_device* dev, IIODeviceAttrType type, const std::string& name,
                     std::string& out) override {
    const iio_attr* attr = find_device_attr(dev, type, name.c_str());
    if (!attr) return -ENOENT;
    char buf[1024];
    ssize_t ret = iio_attr_read_raw(attr, buf, sizeof(buf));
    if (ret >= 0) out = buf;
    return ret;
  }

  ssize_t read_attr(iio_channel* chn, const std::string& name, std::string& out) override {
    const iio_attr* attr = iio_channel_find_attr(chn, name.c_str());
    if (!attr) return -ENOENT;
    char buf[1024];
    ssize_t ret = iio_attr_read_raw(attr, buf, sizeof(buf));
    if (ret >= 0) out = buf;
    return ret;
  }

  ssize_t write_attr(iio_device* dev, IIODeviceAttrType type, const std::string& name,
                      const std::string& value) override {
    const iio_attr* attr = find_device_attr(dev, type, name.c_str());
    if (!attr) return -ENOENT;
    return iio_attr_write_string(attr, value.c_str());
  }

  ssize_t write_attr(iio_channel* chn, const std::string& name,
                      const std::string& value) override {
    const iio_attr* attr = iio_channel_find_attr(chn, name.c_str());
    if (!attr) return -ENOENT;
    return iio_attr_write_string(attr, value.c_str());
  }

 private:
  bool ensure_mask(const iio_device* dev) {
    if (mask_) return true;
    if (!dev) return false;
    unsigned int count = iio_device_get_channels_count(dev);
    mask_ = iio_create_channels_mask(count);
    return mask_ != nullptr;
  }

  iio_channels_mask* mask_ = nullptr;
  iio_buffer_stream* stream_ = nullptr;
  iio_block* block_ = nullptr;
  size_t block_size_ = 0;
  bool cyclic_ = false;
  bool started_ = false;
};

std::unique_ptr<IIOApi> make_iio_api() { return std::make_unique<IIOApiV1>(); }

}  // namespace holoscan::ops
