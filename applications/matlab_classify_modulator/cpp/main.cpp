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

#include <holoscan/holoscan.hpp>
#include <fstream>
#include <iomanip>
#include <ctime>

#include "classifyModulation.h"
#include "classifyModulation_initialize.h"
#include "classifyModulation_terminate.h"
#include "basic_network_operator_rx.h"

namespace holoscan::ops {
class MatlabClassifyModulationOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(MatlabClassifyModulationOp)

  MatlabClassifyModulationOp() = default;

  void setup(OperatorSpec& spec) override {
    spec.input<std::shared_ptr<NetworkOpBurstParams>>("burst_in");
    spec.param(out_file_, "out_file", "Out File Name", "modulation_results.txt");
  }

  void start() override {
    classifyModulation_initialize();

    outfile_.open(out_file_.get(), std::ios_base::out | std::ios_base::trunc);
    if (!outfile_.is_open()) {
      HOLOSCAN_LOG_ERROR("Failed to open output file: {}", out_file_.get());
    }
  }

  void stop() override {
    classifyModulation_terminate();

    if (outfile_.is_open()) {
      outfile_.close();
    }
  }

  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override {
    // Get input message
    auto in_message = op_input.receive<std::shared_ptr<NetworkOpBurstParams>>("burst_in");
    if (!in_message) {
      HOLOSCAN_LOG_WARN("Failed to receive message on 'burst_in'.");
      return;
    }
    auto in_data = in_message.value();
    int16_t* val = reinterpret_cast<int16_t*>(in_data->data);

    float v;
    double modulation;

    // Call MATLAB CUDA function to do modulation classification
    classifyModulation(val, &v, &modulation);

    HOLOSCAN_LOG_INFO("Confidence: {}", v);
    HOLOSCAN_LOG_INFO("Modulation: {}", modulation);

    if (outfile_.is_open()) {
      outfile_.seekp(0, std::ios::beg);

      auto t = std::time(nullptr);
      auto tm = *std::localtime(&t);

      outfile_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << std::endl;
      outfile_ << "Modulation: " << modulation << std::endl;
      outfile_ << "Confidence: " << v << std::endl;

      outfile_.flush();
    }
  }

 private:
  Parameter<std::string> out_file_;
  std::ofstream outfile_;
};

}  // namespace holoscan::ops

class MatlabClassifyModulationApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    // Define operators and configure using yaml configuration
    auto matlab = make_operator<ops::MatlabClassifyModulationOp>(
        "matlab", from_config("matlab"), make_condition<CountCondition>(-1));

    auto net_rx =
        make_operator<ops::BasicNetworkOpRx>("network_rx",
                                             from_config("network_rx"),
                                             make_condition<BooleanCondition>("is_alive", true));

    // Define the workflow
    add_flow(net_rx, matlab, {{"burst_out", "burst_in"}});
  }
};

int main(int argc, char** argv) {
  // Get the yaml configuration file
  auto config_path = std::filesystem::canonical(argv[0]).parent_path();
  config_path /= std::filesystem::path("matlab_classify_modulator.yaml");
  if (argc >= 2) { config_path = argv[1]; }

  auto app = holoscan::make_application<MatlabClassifyModulationApp>();
  app->config(config_path);
  app->run();

  return 0;
}
