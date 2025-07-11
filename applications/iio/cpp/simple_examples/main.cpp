// Copyright 2025 Analog Devices, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <yaml-cpp/exceptions.h>
#include <filesystem>
#include <holoscan/core/conditions/gxf/boolean.hpp>
#include <holoscan/core/conditions/gxf/count.hpp>
#include <holoscan/core/endpoint.hpp>
#include <holoscan/core/forward_def.hpp>
#include <holoscan/core/operator.hpp>
#include "holoscan/holoscan.hpp"
#include "iio_attribute_read.hpp"
#include "iio_attribute_write.hpp"
#include "iio_buffer_read.hpp"
#include "iio_buffer_write.hpp"
#include "iio_configurator.hpp"
#include "iio_params.hpp"
#include "support_operators.hpp"

#include <dlpack/dlpack.h>

class IIOSimpleExamplesApp : public holoscan::Application {
 public:
  IIOSimpleExamplesApp() { name_ = "IIO Simple Examples"; }

  void attr_read_example() {
    using namespace holoscan;
    HOLOSCAN_LOG_INFO("Setting up attribute read example");

    auto iio_read_cond = make_condition<CountCondition>("iio_read_cond", G_NUM_READS);

    auto iio_read_op =
        make_operator<ops::IIOAttributeRead>("iio_attribute_read",
                                             Arg("ctx") = std::string(G_URI),
                                             Arg("dev") = std::string("ad9361-phy"),
                                             Arg("attr_name") = std::string("trx_rate_governor"),
                                             iio_read_cond);

    auto basic_print_op = make_operator<ops::BasicPrinterOp>("basic_print_op");

    add_flow(iio_read_op, basic_print_op, {{"value", "value"}});
  }

  void attr_write_example() {
    using namespace holoscan;
    HOLOSCAN_LOG_INFO("Setting up attribute write example");

    auto iio_write_cond = make_condition<CountCondition>("iio_write_cond", G_NUM_READS);

    auto iio_write_op =
        make_operator<ops::IIOAttributeWrite>("iio_attribute_write",
                                              Arg("ctx") = std::string(G_URI),
                                              Arg("dev") = std::string("ad9361-phy"),
                                              Arg("attr_name") = std::string("trx_rate_governor"),
                                              iio_write_cond);

    auto basic_emit_op = make_operator<ops::BasicEmitterOp>("basic_emit_op");

    add_flow(basic_emit_op, iio_write_op, {{"value", "value"}});
  }

  void buffer_read_example() {
    using namespace holoscan;
    HOLOSCAN_LOG_INFO("Setting up buffer read example");

    auto iio_rw_cond = make_condition<CountCondition>("iio_read_cond", 1);

    // Channel configuration based on G_NUM_CHANNELS
    std::vector<std::string> enabled_channels_names_1;
    std::vector<bool> enabled_channels_output;

    enabled_channels_names_1.push_back("voltage0");
    enabled_channels_output.push_back(false);  // False for input channels

    if (G_NUM_CHANNELS == 2) {
      enabled_channels_names_1.push_back("voltage1");
      enabled_channels_output.push_back(false);
    }

    auto iio_buf_read_op =
        make_operator<ops::IIOBufferRead>("iio_buffer_read",
                                          Arg("ctx") = std::string(G_URI),
                                          Arg("dev") = std::string("cf-ad9361-lpc"),
                                          Arg("is_cyclic") = true,
                                          Arg("samples_count") = static_cast<size_t>(8192),
                                          Arg("enabled_channel_names") = enabled_channels_names_1,
                                          Arg("enabled_channel_output") = enabled_channels_output,
                                          iio_rw_cond);

    auto basic_buffer_printer_op =
        make_operator<ops::BasicIIOBufferPrinterOP>("basic_buffer_printer_op");

    // RX flow - connect buffer reader to buffer printer
    add_flow(iio_buf_read_op, basic_buffer_printer_op, {{"buffer", "buffer"}});
  }

  void buffer_write_example() {
    using namespace holoscan;
    HOLOSCAN_LOG_INFO("Setting up buffer write example");

    auto iio_rw_cond = make_condition<CountCondition>("iio_write_cond", 1);

    // Channel configuration based on G_NUM_CHANNELS
    std::vector<std::string> enabled_channels_names_1;
    std::vector<bool> enabled_channels_output;

    enabled_channels_names_1.push_back("voltage0");
    enabled_channels_output.push_back(true);  // True for output channels

    if (G_NUM_CHANNELS == 2) {
      enabled_channels_names_1.push_back("voltage1");
      enabled_channels_output.push_back(true);
    }

    auto iio_buf_write_op_1 =
        make_operator<ops::IIOBufferWrite>("iio_buffer_write_1",
                                           Arg("ctx") = std::string(G_URI),
                                           Arg("dev") = std::string("cf-ad9361-dds-core-lpc"),
                                           Arg("is_cyclic") = true,
                                           Arg("enabled_channel_names") = enabled_channels_names_1,
                                           Arg("enabled_channel_output") = enabled_channels_output,
                                           iio_rw_cond);

    auto basic_buffer_emitter_op =
        make_operator<ops::BasicIIOBufferEmitterOP>("basic_buffer_emitter_op");

    auto basic_wait_op = make_operator<ops::BasicWaitOp>("basic_wait_op");

    // TX flow - connect buffer emitter to buffer writer to wait
    add_flow(basic_buffer_emitter_op, iio_buf_write_op_1, {{"buffer", "buffer"}});
    add_flow(iio_buf_write_op_1, basic_wait_op);
  }

  void configurator_example() {
    using namespace holoscan;
    HOLOSCAN_LOG_INFO("Setting up configurator example");

    auto config_file_path = config().config_file();
    HOLOSCAN_LOG_INFO("Config file: {}", config_file_path);

    auto iio_configurator_op = make_operator<ops::IIOConfigurator>(
        "iio_configurator_op", Arg("cfg") = std::string(config_file_path));

    // start_op() will only run the configurator once
    add_flow(start_op(), iio_configurator_op);
  }

  void compose() override {
    HOLOSCAN_LOG_INFO("IIO Simple Examples started");

    // Uncomment the examples you want to run
    // attr_read_example();
    // attr_write_example();
    // buffer_write_example();
    buffer_read_example();
    // configurator_example();
  }

 private:
  std::string name_;
};

int simple_examples_main(int argc, char** argv) {
  // Set the yaml config path
  // auto config_path = std::filesystem::canonical(argv[0]).parent_path();
  // config_path /= std::filesystem::path("iio_config.yaml");
  // if (argc > 1) {
  //   config_path = std::filesystem::path(argv[1]);
  // }
  //
  // auto app = holoscan::make_application<IIOSimpleExamplesApp>();
  // app->config(config_path);
  // app->run();

  auto app = holoscan::make_application<IIOSimpleExamplesApp>();
  app->run();

  return 0;
}