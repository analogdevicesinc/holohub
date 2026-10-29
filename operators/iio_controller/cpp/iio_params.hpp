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
#include <string>
#include <vector>
#include "iio_api.hpp"

enum class attr_type_t {
  CONTEXT,
  DEVICE,
  CHANNEL,
  UNKNOWN,
};

struct iio_channel_info_t {
  std::string name;
  bool is_output;
  unsigned int index;                   // Channel index
  holoscan::ops::IIODataFormat format;  // Channel data format
};

struct iio_buffer_info_t {
  size_t samples_count;
  void* buffer;
  std::vector<iio_channel_info_t> enabled_channels;
  bool is_cyclic;
  std::string device_name;
};

/**
 * @brief Creates an iio_channel_info_t structure from an IIO channel pointer
 *
 * This helper function simplifies the creation of channel information structures
 * by automatically extracting all relevant data from an IIO channel, including
 * the channel name, direction (input/output), index, and data format.
 *
 * This is particularly useful when:
 * - Building buffer information for IIOBufferWrite operations
 * - Creating channel metadata for custom operators
 * - Needing complete channel information without manual field population
 *
 * @param api IIOApi instance used to query the channel
 * @param channel Pointer to the IIO channel (must not be null)
 * @return iio_channel_info_t Complete channel information structure
 *
 * @note If the channel pointer is null, returns a default-initialized structure
 */
inline iio_channel_info_t create_channel_info_from_iio_channel(holoscan::ops::IIOApi& api,
                                                                const iio_channel* channel) {
  iio_channel_info_t info;

  if (!channel) {
    // Return default-initialized struct if channel is null
    info.name = "";
    info.is_output = false;
    info.index = 0;
    info.format = {};
    return info;
  }

  info.name = api.channel_id(channel);
  info.is_output = api.channel_is_output(channel);

  long idx = api.channel_index(channel);
  info.index = (idx >= 0) ? static_cast<unsigned int>(idx) : 0;

  info.format = api.channel_format(channel);

  return info;
}
