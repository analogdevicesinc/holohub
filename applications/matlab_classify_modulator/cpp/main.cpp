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

#include "classifyModulation.h"
#include "classifyModulation_terminate.h"
#include "basic_network_operator_rx.h"

namespace holoscan::ops {
class MatlabClassifyModulationOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(MatlabClassifyModulationOp)

  MatlabClassifyModulationOp() = default;

  void setup(OperatorSpec& spec) override {
    spec.input<std::shared_ptr<NetworkOpBurstParams>>("burst_in");
  }

  void start() {
  }

  void stop() {
    classifyModulation_terminate();
  }

  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override {
    float v;
    double modulation;

    // Get input message
    auto in = op_input.receive<std::shared_ptr<NetworkOpBurstParams>>("burst_in").value();
    creal32_T* val = reinterpret_cast<creal32_T*>(in->data);

    // Call MATLAB CUDA function to do image processing
    classifyModulation(val, &v, &modulation);

    // Create output message
    HOLOSCAN_LOG_INFO("Confidence: {}", v);
    HOLOSCAN_LOG_INFO("Modulation: {}", modulation);

    delete[] in->data;

    //auto result = modulation;
    //op_output.emit(result);
  }

 private:
};

}  // namespace holoscan::ops

class MatlabClassifyModulationApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    // Define operators and configure using yaml configuration
    auto matlab = make_operator<ops::MatlabClassifyModulationOp>("matlab", make_condition<CountCondition>(10));

    auto net_rx = make_operator<ops::BasicNetworkOpRx>(
        "network_rx", from_config("network_rx"), make_condition<BooleanCondition>("is_alive"));

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
