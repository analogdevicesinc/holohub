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

#include "iio_buffer_write.hpp"
#include <gxf/core/gxf.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <holoscan/logger/logger.hpp>
#include <memory>
#include "iio_params.hpp"

using namespace holoscan::ops;

void IIOBufferWrite::stop() {
  if (buffer_open_) {
    api_->close_buffer();
    buffer_open_ = false;
  }
  if (ctx_) {
    api_->destroy_context(ctx_);
    ctx_ = nullptr;
  }
}

void IIOBufferWrite::setup(OperatorSpec& spec) {
  HOLOSCAN_LOG_INFO("IIOBufferWrite setup");
  spec.input<std::shared_ptr<iio_buffer_info_t>>("buffer");

  spec.param<std::string>(ctx_p_, "ctx", "IIO Context", "The URI of the IIO Context");
  spec.param<std::string>(dev_p_, "dev", "IIO Device", "Name of the IIO Device");
  spec.param<bool>(is_cyclic, "is_cyclic", "Is cyclic", "Is the buffer cyclic?");
  spec.param<std::vector<std::string>>(
      enabled_channel_names_p_,
      "enabled_channel_names",
      "Names of the enabled IIO Channels",
      "The names of the channels that are enabled when pushing the buffer");
  spec.param<std::vector<bool>>(
      enabled_channel_types_p_,
      "enabled_channel_output",
      "Types of the enabled IIO Channels",
      "The types of the channels that are enabled when pushing the buffer");
}

void IIOBufferWrite::initialize() {
  HOLOSCAN_LOG_INFO("IIOBufferWrite initialize");
  Operator::initialize();

  if (enabled_channel_names_p_.get().empty() || enabled_channel_types_p_.get().empty()) {
    HOLOSCAN_LOG_ERROR(
        "It is mandatory to enable at least one channel before creating the "
        "IIO buffer.");
    channels_empty_ = true;
    return;
  }

  ctx_ = api_->create_context(ctx_p_.get());
  if (!ctx_) {
    HOLOSCAN_LOG_ERROR("Failed to create IIO context from URI: {}", ctx_p_.get());
    ctx_creation_failed_ = true;
    return;
  }

  dev_ = api_->find_device(ctx_, dev_p_.get());
  if (!dev_) {
    HOLOSCAN_LOG_ERROR("Failed to find IIO device: {}", dev_p_.get());
    dev_not_found_ = true;
    return;
  }

  std::vector<std::string>& enabled_channel_names = enabled_channel_names_p_.get();
  std::vector<bool>& enabled_channel_types = enabled_channel_types_p_.get();
  for (size_t i = 0; i < enabled_channel_names.size(); ++i) {
    std::string& chn_name = enabled_channel_names[i];
    bool chn_type = enabled_channel_types[i];
    iio_channel* chn = api_->find_channel(dev_, chn_name, chn_type);
    if (!chn) {
      HOLOSCAN_LOG_ERROR("Failed to find {} channel {} from device {}",
                         chn_type ? "output" : "input",
                         chn_name,
                         dev_p_.get());
      chan_not_found_ = true;
      return;
    }

    if (!api_->is_channel_enabled(chn)) {
      HOLOSCAN_LOG_INFO("Enabling channel: {}", chn_name);
    } else {
      HOLOSCAN_LOG_INFO("Channel {} is already enabled", chn_name);
    }
    api_->enable_channel(chn);
    HOLOSCAN_LOG_INFO("Enabled channel: {}", chn_name);
  }

  // NOTE: Get sample size only after enabling the channels
  sample_size_ = api_->sample_size(dev_);
  if (sample_size_ < 0) {
    HOLOSCAN_LOG_ERROR(
        "Failed to get sample size from device {}; Err code: {}", dev_p_.get(), sample_size_);
    sample_size_failed_ = true;
    return;
  }

  buffer_open_ = false;
}

void IIOBufferWrite::compute(InputContext& op_input, OutputContext&, ExecutionContext& context) {
  HOLOSCAN_LOG_INFO("IIOBufferWrite compute");

  // Check if initialization failed and interrupt graph execution
  if (channels_empty_) {
    HOLOSCAN_LOG_ERROR("Cannot proceed: No channels enabled for IIO buffer");
    GxfGraphInterrupt(context.context());
    return;
  }
  if (ctx_creation_failed_) {
    HOLOSCAN_LOG_ERROR("Cannot proceed: IIO context creation failed for URI: {}", ctx_p_.get());
    GxfGraphInterrupt(context.context());
    return;
  }
  if (dev_not_found_) {
    HOLOSCAN_LOG_ERROR("Cannot proceed: IIO device '{}' not found", dev_p_.get());
    GxfGraphInterrupt(context.context());
    return;
  }
  if (chan_not_found_) {
    HOLOSCAN_LOG_ERROR("Cannot proceed: One or more IIO channels not found");
    GxfGraphInterrupt(context.context());
    return;
  }
  if (sample_size_failed_) {
    HOLOSCAN_LOG_ERROR("Cannot proceed: Failed to get sample size from device '{}'", dev_p_.get());
    GxfGraphInterrupt(context.context());
    return;
  }
  auto buffer_info = op_input.receive<std::shared_ptr<iio_buffer_info_t>>("buffer").value();

  if (buffer_info->buffer == nullptr) {
    HOLOSCAN_LOG_ERROR("Buffer is null");
    return;
  }

  // Log the received buffer information for debugging
  HOLOSCAN_LOG_DEBUG("Received buffer from device: {}, samples: {}, cyclic: {}, channels: {}",
                     buffer_info->device_name,
                     buffer_info->samples_count,
                     buffer_info->is_cyclic,
                     buffer_info->enabled_channels.size());

  if (!buffer_open_ || buffer_samples_count_ != buffer_info->samples_count) {
    if (buffer_open_) { HOLOSCAN_LOG_INFO("Destroying buffer due to size mismatch."); }

    HOLOSCAN_LOG_INFO("Creating new buffer with {} samples of size {} bytes.",
                      buffer_info->samples_count,
                      sample_size_);
    buffer_samples_count_ = static_cast<uint32_t>(buffer_info->samples_count);
    buffer_open_ =
        api_->open_buffer(dev_, buffer_info->samples_count, buffer_info->is_cyclic);
    if (!buffer_open_) {
      HOLOSCAN_LOG_ERROR("Failed to create buffer, error code {}", errno);
      GxfGraphInterrupt(context.context());
      return;
    }
  }

  // Copy the buffer data to the IIO buffer and push it
  ssize_t res = api_->push(buffer_info->buffer,
                           buffer_info->samples_count * static_cast<size_t>(sample_size_));
  if (res < 0) {
    HOLOSCAN_LOG_ERROR("Failed to push buffer. Error code: {}", res);
    return;
  }
}
