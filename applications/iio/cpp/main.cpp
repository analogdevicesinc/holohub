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

#include <holoscan/core/conditions/gxf/boolean.hpp>
#include <holoscan/core/conditions/gxf/count.hpp>
#include <holoscan/core/endpoint.hpp>
#include <holoscan/core/forward_def.hpp>
#include <holoscan/core/operator.hpp>
#include "holoscan/holoscan.hpp"
#include "pluto_fft_example/pluto_fft_example.hpp"

// Forward declaration for simple examples main
int simple_examples_main(int argc, char** argv);

int main(int argc, char** argv) {
  // Check command line arguments
  bool realtime = false;
  bool simple_examples = false;

  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--realtime") {
      realtime = true;
    } else if (std::string(argv[i]) == "--simple-examples") {
      simple_examples = true;
    } else if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
      std::cout << "Usage: " << argv[0] << " [options]\n";
      std::cout << "Options:\n";
      std::cout << "  --realtime         Run Pluto FFT example in real-time mode\n";
      std::cout << "  --simple-examples  Run simple IIO examples\n";
      std::cout << "  --help, -h         Show this help message\n";
      std::cout << "\nDefault: Run Pluto FFT example in single-shot mode\n";
      return 0;
    }
  }

  if (simple_examples) {
    simple_examples_main(argc, argv);
  } else if (realtime) {
    pluto_fft_realtime_main(argc, argv);
  } else {
    pluto_fft_main(argc, argv);
  }

  return 0;
}