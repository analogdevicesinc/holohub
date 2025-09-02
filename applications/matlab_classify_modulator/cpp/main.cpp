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

#include <cstring>
#include <holoscan/holoscan.hpp>
#include <memory>

#include "classifyModulation.h"
#include "classifyModulation_terminate.h"
#include "iio_buffer_read.hpp"
#include "iio_params.hpp"

namespace holoscan::ops {

// Adapter operator to convert IIO buffer format to raw data format expected by MATLAB
class IIOToMatlabAdapterOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(IIOToMatlabAdapterOp)

  IIOToMatlabAdapterOp() = default;

  void setup(OperatorSpec& spec) override {
    spec.input<std::shared_ptr<iio_buffer_info_t>>("buffer_in");
    spec.output<int16_t*>("data_out");
  }

  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override {
    // Get input buffer from IIO
    auto buffer_info = op_input.receive<std::shared_ptr<iio_buffer_info_t>>("buffer_in").value();

    // The buffer contains interleaved I/Q data for all enabled channels
    // Cast the buffer to int16_t* for MATLAB processing
    int16_t* data = reinterpret_cast<int16_t*>(buffer_info->buffer);

    // Create a copy of the data that will be managed by this operator
    size_t data_size = buffer_info->samples_count * buffer_info->enabled_channels.size();
    int16_t* matlab_data = new int16_t[data_size];
    std::memcpy(matlab_data, data, data_size * sizeof(int16_t));

    // Output the raw data pointer for MATLAB
    op_output.emit(matlab_data, "data_out");
  }
};

class MatlabClassifyModulationOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(MatlabClassifyModulationOp)

  MatlabClassifyModulationOp() = default;

  Parameter<std::string> out_file_;
  std::ofstream outfile_;

  void setup(OperatorSpec& spec) override {
    spec.input<int16_t*>("data_in");
    spec.param<std::string>(out_file_, "out_file", "Out File Name", "modulation_results.txt");
  }

  void start() { outfile_.open(out_file_.get(), std::ios_base::out); }

  void stop() {
    classifyModulation_terminate();
    outfile_.close();
  }

  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override {
    std::vector<float> v(5);
    std::vector<double> modulation(5);

    // Get input data
    auto val = op_input.receive<int16_t*>("data_in").value();

    // Call MATLAB CUDA function to do modulation classification
    for (int i = 1; i < 5; i++) { classifyModulation(val, i, &v[i], &modulation[i]); }

    // Create output message
    HOLOSCAN_LOG_INFO("Confidence {}", v);
    // Log modulation information
    HOLOSCAN_LOG_INFO("Modulation {}", modulation);

    // Move file pointer to the beginning
    outfile_.seekp(0, std::ios::beg);

    // Save i, v, and modulation to a txt file
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    outfile_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << std::endl;
    for (int i = 1; i < 5; i++) { 
	std::cout << i << ": " << modulation[i] << std::endl;
	    outfile_ << modulation[i] << std::endl; 
    }
    for (int i = 1; i < 5; i++) { outfile_ << v[i] << std::endl; }

    // Clean up the data allocated by adapter
    delete[] val;

    outfile_.flush();
  }

 private:
};

}  // namespace holoscan::ops

class MatlabClassifyModulationApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    // Define operators and configure using yaml configuration
    auto matlab_1 = make_operator<ops::MatlabClassifyModulationOp>(
        "matlab_1", Arg("out_file") = std::string("modulation_results.txt"), make_condition<CountCondition>(-1));

    auto matlab_2 = make_operator<ops::MatlabClassifyModulationOp>(
        "matlab_2", Arg("out_file") = std::string("modulation_results1.txt"), make_condition<CountCondition>(-1));

    // Configure IIO buffer read for Talise device at IP 10.43.1.10
    // Using axi-adrv9009-rx-hpc device for RX data
    std::vector<std::string> channel_names = {
	    "voltage0_i", "voltage0_q", 
	    "voltage1_i", "voltage1_q",
	    "voltage2_i", "voltage2_q",
	    "voltage3_i", "voltage3_q"
    };
    std::vector<bool> channel_types = {false, false, false, false, false, false, false, false};  // All are input channels

    auto iio_rx_1 =
        make_operator<ops::IIOBufferRead>("iio_rx_1",
                                          Arg("ctx") = std::string("ip:10.43.1.10"),
                                          Arg("dev") = std::string("axi-adrv9009-rx-hpc"),
                                          Arg("is_cyclic") = true,
                                          Arg("samples_count") = static_cast<size_t>(8192),
                                          Arg("enabled_channel_names") = channel_names,
                                          Arg("enabled_channel_output") = channel_types,
                                          make_condition<BooleanCondition>("is_alive", true));

    auto iio_rx_2 =
        make_operator<ops::IIOBufferRead>("iio_rx_2",
                                          Arg("ctx") = std::string("ip:10.43.0.10"),
                                          Arg("dev") = std::string("axi-adrv9009-rx-hpc"),
                                          Arg("is_cyclic") = true,
                                          Arg("samples_count") = static_cast<size_t>(8192),
                                          Arg("enabled_channel_names") = channel_names,
                                          Arg("enabled_channel_output") = channel_types,
                                          make_condition<BooleanCondition>("is_alive", true));

    // Create adapter operator to convert IIO buffer to MATLAB format
    auto adapter_1 = make_operator<ops::IIOToMatlabAdapterOp>("iio_adapter_1");
    auto adapter_2 = make_operator<ops::IIOToMatlabAdapterOp>("iio_adapter_2");

    // Define the workflow: IIO -> Adapter -> MATLAB
    add_flow(iio_rx_1, adapter_1, {{"buffer", "buffer_in"}});
    add_flow(adapter_1, matlab_1, {{"data_out", "data_in"}});

    add_flow(iio_rx_2, adapter_2, {{"buffer", "buffer_in"}});
    add_flow(adapter_2, matlab_2, {{"data_out", "data_in"}});
  }
};

int main(int argc, char** argv) {
  // Get the yaml configuration file
  auto config_path = std::filesystem::canonical(argv[0]).parent_path();
  config_path /= std::filesystem::path("matlab_classify_modulator.yaml");
  std::cout << "config path: " << config_path << std::endl;
  // if (argc >= 2) {
  //   config_path = argv[1];
  // }

  auto app = holoscan::make_application<MatlabClassifyModulationApp>();
  app->config(config_path);
  app->run();

  return 0;
}
