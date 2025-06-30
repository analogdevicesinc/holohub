/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "adv_network_rx.h"
#include "holoscan/holoscan.hpp"
#include <fstream>
#include <iomanip>
#include <ctime>

#include "classifyModulation.h"
#include "classifyModulation_terminate.h"

namespace holoscan::ops {

#define CUDA_TRY(stmt)                                                                          \
  ({                                                                                            \
    cudaError_t _holoscan_cuda_err = stmt;                                                      \
    if (cudaSuccess != _holoscan_cuda_err) {                                                    \
      HOLOSCAN_LOG_ERROR("CUDA Runtime call %s in line %d of file %s failed with '%s' (%d).\n", \
                         #stmt,                                                                 \
                         __LINE__,                                                              \
                         __FILE__,                                                              \
                         cudaGetErrorString(_holoscan_cuda_err),                                \
                         static_cast<int>(_holoscan_cuda_err));                                 \
    }                                                                                           \
    _holoscan_cuda_err;                                                                         \
  })

/**
 * @brief Operator to receive data using the Advanced Networking Operator and
 *        process it with a MATLAB function.
 *
 * It receives scattered packets from the AdvNetRx operator, gathers their payloads into
 * a single contiguous host buffer, and then passes this buffer to the `classifyModulation`
 * function.
 */
class AdvNetworkingDefaultRxOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(AdvNetworkingDefaultRxOp)

  AdvNetworkingDefaultRxOp() = default;

  ~AdvNetworkingDefaultRxOp() {
    HOLOSCAN_LOG_INFO("ANO RX operator shutting down.");
    HOLOSCAN_LOG_INFO("Finished receiver with {} bytes and {} packets received.",
                      total_bytes_received_,
                      total_packets_received_);
  }

  void setup(OperatorSpec& spec) override {
    spec.input<AdvNetBurstParams*>("burst_in");

    // Parameters for packet structure
    spec.param<uint32_t>(batch_size_,
                         "batch_size",
                         "Batch Size",
                         "Number of packets to aggregate into one frame.",
                         1000);
    spec.param<uint16_t>(max_packet_size_,
                         "max_packet_size",
                         "Max Packet Size",
                         "Maximum size of a single packet in bytes.",
                         9100);
    spec.param<uint16_t>(header_size_,
                         "header_size",
                         "Header Size",
                         "Size of the network headers (e.g., Eth/IP/UDP) in bytes.",
                         42);
    spec.param<std::string>(
      out_file_, "out_file", "Out File Name", "modulation_results_advanced.txt");
  }

  void initialize() override {
    holoscan::Operator::initialize();

    // Calculate the maximum possible size for the payload of a full batch.
    uint16_t payload_size_per_packet = max_packet_size_.get() - header_size_.get();
    uint64_t max_buffer_size = (uint64_t)batch_size_.get() * payload_size_per_packet;

    // Allocate a single, contiguous, page-locked (pinned) host buffer.
    // This buffer will be used to gather the payloads from the scattered packets.
    cudaError_t cuda_error =
        CUDA_TRY(cudaMallocHost(&host_buffer_, max_buffer_size));
    if (cudaSuccess != cuda_error) {
      throw std::runtime_error("Failed to allocate pinned host memory for gathering packets.");
    }
    HOLOSCAN_LOG_INFO("Allocated {} bytes of pinned host memory for packet aggregation.", max_buffer_size);
  }

  void start() override {
    outfile_.open(out_file_.get(), std::ios_base::out);
    if (!outfile_.is_open()) {
      HOLOSCAN_LOG_ERROR("Failed to open output file: {}", out_file_.get());
    }
  }

  void stop() override {
    classifyModulation_terminate();
    if (outfile_.is_open()) {
      outfile_.close();
    }
    if (host_buffer_) {
      CUDA_TRY(cudaFreeHost(host_buffer_));
      host_buffer_ = nullptr;
    }
  }

  void compute(InputContext& op_input, OutputContext&, ExecutionContext&) override {
    // Receive the burst of packets from the advanced network operator.
    auto burst_opt = op_input.receive<AdvNetBurstParams*>("burst_in");
    if (!burst_opt) {
      // No data received in this call.
      return;
    }

    AdvNetBurstParams* burst = burst_opt.value();
    const uint32_t num_packets_in_burst = adv_net_get_num_pkts(burst);
    total_packets_received_ += num_packets_in_burst;

    if (num_packets_in_burst == 0) {
      // Received an empty burst, just free it and return.
      adv_net_free_all_pkts_and_burst(burst);
      return;
    }

    // Gather scattered packet payloads into our single contiguous host buffer.
    size_t current_buffer_offset = 0;
    const uint16_t header_sz = header_size_.get();
    const uint64_t max_buffer_size = (uint64_t)batch_size_.get() * (max_packet_size_.get() - header_sz);


    for (uint32_t i = 0; i < num_packets_in_burst; ++i) {
      // Assuming non-HDS, single-segment packets (all data is in segment 0)
      const uint8_t* pkt_data = static_cast<const uint8_t*>(adv_net_get_seg_pkt_ptr(burst, 0, i));
      const size_t pkt_len = adv_net_get_seg_pkt_len(burst, 0, i);
      total_bytes_received_ += pkt_len;

      if (pkt_len <= header_sz) {
        HOLOSCAN_LOG_WARN("Skipping packet with size {} <= header size {}", pkt_len, header_sz);
        continue;
      }

      const uint8_t* payload_ptr = pkt_data + header_sz;
      const size_t payload_len = pkt_len - header_sz;

      // Prevent buffer overflow
      if (current_buffer_offset + payload_len > max_buffer_size) {
        HOLOSCAN_LOG_ERROR("Buffer overflow detected while gathering packets. Dropping remaining packets in burst.");
        break;
      }

      // Copy payload into the contiguous buffer
      memcpy(static_cast<uint8_t*>(host_buffer_) + current_buffer_offset, payload_ptr, payload_len);
      current_buffer_offset += payload_len;
    }

    // The original burst is no longer needed, free it to release buffers back to the NIC.
    adv_net_free_all_pkts_and_burst(burst);

    int16_t* val = reinterpret_cast<int16_t*>(host_buffer_);
    float v;
    double modulation;

    // Call MATLAB CUDA function to do modulation classification
    classifyModulation(val, &v, &modulation);

    HOLOSCAN_LOG_INFO("Confidence {}", v);
    HOLOSCAN_LOG_INFO("Modulation {}", modulation);

    outfile_.seekp(0, std::ios::beg);

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    outfile_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << std::endl;
    outfile_ << modulation << std::endl;
    outfile_ << v << std::endl;

    outfile_.flush();
  }

 private:
  // Buffer to gather scattered network data into a single contiguous block
  void* host_buffer_ = nullptr;

  // Parameters
  Parameter<uint32_t> batch_size_;
  Parameter<uint16_t> max_packet_size_;
  Parameter<uint16_t> header_size_;
  Parameter<std::string> out_file_;

  // File stream for results
  std::ofstream outfile_;

  // Statistics
  uint64_t total_packets_received_ = 0;
  uint64_t total_bytes_received_ = 0;
};

}  // namespace holoscan::ops
