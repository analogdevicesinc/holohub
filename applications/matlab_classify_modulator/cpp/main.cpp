/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 ADI Inc.
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

#include <cstdio>
#include <ctime>
#include <fstream>
#include <holoscan/holoscan.hpp>
#include <iomanip>
#include <string>

#include "classifyModulation.h"
#include "classifyModulation_initialize.h"
#include "classifyModulation_terminate.h"
#include "iio_buffer_read.hpp"
#include "iio_params.hpp"

static constexpr const char* G_URI = "ip:192.168.2.1";

namespace holoscan::ops {
class MatlabClassifyModulationOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(MatlabClassifyModulationOp)

  MatlabClassifyModulationOp() = default;

  void setup(OperatorSpec& spec) override {
    spec.input<std::shared_ptr<iio_buffer_info_t>>("buffer_in");
    spec.param(out_file_, "out_file", "Out File Name", "modulation_results.txt");
  }

  void start() override {
    classifyModulation_initialize();
    // No need to open file here - we'll use atomic writes in compute()
  }

  void stop() override {
    classifyModulation_terminate();
    // No file to close - using atomic writes
  }

  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override {
    // Get input message
    auto in_message = op_input.receive<std::shared_ptr<iio_buffer_info_t>>("buffer_in");
    if (!in_message) {
      HOLOSCAN_LOG_WARN("Failed to receive message on 'buffer_in'.");
      return;
    }
    auto in_data = in_message.value();
    int16_t* val = reinterpret_cast<int16_t*>(in_data->buffer);

    float v;
    double modulation;

    // Increment sequence number for each classification
    sequence_number_++;

    // Call MATLAB CUDA function to do modulation classification
    classifyModulation(val, &v, &modulation);

    // MATLAB returns 1-based index (1-8)
    int matlab_index = static_cast<int>(modulation);

    HOLOSCAN_LOG_INFO("Confidence: {}", v);
    HOLOSCAN_LOG_INFO("Modulation: {} ", modulation);

    // Write to temporary file first, then rename atomically
    std::string temp_file = out_file_.get() + ".tmp";
    std::ofstream temp_out(temp_file, std::ios_base::out | std::ios_base::trunc);

    if (temp_out.is_open()) {
      auto t = std::time(nullptr);
      auto tm = *std::localtime(&t);

      temp_out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << std::endl;
      temp_out << "Sequence: " << sequence_number_ << std::endl;
      temp_out << "Modulation: " << modulation << std::endl;
      temp_out << "Confidence: " << v << std::endl;

      temp_out.close();

      // Atomic rename - this ensures Python always reads a complete file
      std::rename(temp_file.c_str(), out_file_.get().c_str());
    }
  }

 private:
  Parameter<std::string> out_file_;
  uint64_t sequence_number_ = 0;

  static constexpr const char* modulation_labels[] = {
      "16QAM", "64QAM", "8PSK", "BPSK", "CPFSK", "GFSK", "PAM4", "QPSK"};
  static constexpr size_t num_modulations =
      sizeof(modulation_labels) / sizeof(modulation_labels[0]);
};

}  // namespace holoscan::ops

class MatlabClassifyModulationApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    // Define operators and configure using yaml configuration
    // FIXME: Fix the config file (path ig) so that the out_file is read from there
    auto matlab = make_operator<ops::MatlabClassifyModulationOp>(
        "matlab", Arg("out_file") = std::string("modulation_results.txt"));

    // Channel configuration for PlutoSDR
    std::vector<std::string> enabled_channels_names;
    std::vector<bool> enabled_channels_output;

    enabled_channels_names.push_back("voltage0");
    enabled_channels_output.push_back(false);  // False for input channels
    enabled_channels_names.push_back("voltage1");
    enabled_channels_output.push_back(false);

    auto iio_buffer_reader =
        make_operator<ops::IIOBufferRead>("iio_buffer_reader",
                                          Arg("ctx") = std::string(G_URI),
                                          Arg("dev") = std::string("cf-ad9361-lpc"),
                                          Arg("is_cyclic") = true,
                                          Arg("samples_count") = static_cast<size_t>(8192),
                                          Arg("enabled_channel_names") = enabled_channels_names,
                                          Arg("enabled_channel_output") = enabled_channels_output);

    // Define the workflow
    add_flow(iio_buffer_reader, matlab, {{"buffer", "buffer_in"}});
  }
};

int main(int argc, char** argv) {
  // Get the yaml configuration file
  auto config_path = std::filesystem::canonical(argv[0]).parent_path();
  config_path /= std::filesystem::path("matlab_classify_modulator.yaml");
  if (argc >= 2) {
    config_path = argv[1];
  }

  auto app = holoscan::make_application<MatlabClassifyModulationApp>();
  app->config(config_path);
  app->run();

  return 0;
}
