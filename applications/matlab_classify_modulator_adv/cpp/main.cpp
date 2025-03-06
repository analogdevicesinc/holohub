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

#include "default_op_rx.h"
#include "adv_network_kernels.h"
#include <holoscan/holoscan.hpp>

#include "classifyModulation_terminate.h"

namespace holoscan::ops {
class MatlabClassifyModulationOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(MatlabClassifyModulationOp)

  MatlabClassifyModulationOp() = default;

  void setup(OperatorSpec& spec) override {
    spec.input<std::shared_ptr<AdvNetBurstParams>>("burst_in");
  }

  void start() {
  }

  void stop() {
    classifyModulation_terminate();
  }

  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override {
    //Do nothing
    //Processing moved to default_op_rx.h
  }

 private:
};

}  // namespace holoscan::ops

class MatlabClassifyModulationApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    auto output_rx_ports = holoscan::ops::adv_net_get_port_names(config(), "rx");
    auto adv_net_rx =
      make_operator<ops::AdvNetworkOpRx>("adv_network_rx", output_rx_ports, from_config("advanced_network"),
                                          make_condition<BooleanCondition>("is_alive", true));

    auto bench_rx =
      make_operator<ops::AdvNetworkingDefaultRxOp>("bench_rx", from_config("bench_rx"));

    // Define operators and configure using yaml configuration
    auto matlab = make_operator<ops::MatlabClassifyModulationOp>("matlab", make_condition<CountCondition>(-1));

    // Define the workflow
    add_flow(adv_net_rx, bench_rx, {{ "adv_rx_out", "burst_in" }});
    add_flow(bench_rx, matlab, {{"burst_out", "burst_in"}});
  }
};

int main(int argc, char** argv) {
  // Get the yaml configuration file
  auto config_path = std::filesystem::canonical(argv[0]).parent_path();
  config_path /= std::filesystem::path("matlab_classify_modulator_adv.yaml");
  if (argc >= 2) { config_path = argv[1]; }

  auto app = holoscan::make_application<MatlabClassifyModulationApp>();
  app->config(config_path);
  app->scheduler(app->make_scheduler<holoscan::MultiThreadScheduler>(
      "multithread-scheduler", app->from_config("scheduler")));
  app->run();

  holoscan::ops::adv_net_shutdown();
  return 0;
}
