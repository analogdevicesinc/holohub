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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <holoscan/core/fragment.hpp>
#include <holoscan/core/operator.hpp>
#include <holoscan/core/operator_spec.hpp>

#include "../../operator_util.hpp"
#include "../cpp/monopulse_tracker.hpp"
#include "monopulse_tracker_pydoc.hpp"

using std::string_literals::operator""s;
using pybind11::literals::operator""_a;

namespace py = pybind11;

namespace holoscan::ops {

// Python wrapper class
class PyMonopulseTracker : public MonopulseTracker {
 public:
  using MonopulseTracker::MonopulseTracker;

  // Constructor with Python args
  PyMonopulseTracker(Fragment* fragment,
                     const py::args& args,
                     float element_spacing,
                     float signal_frequency,
                     float phase_step = 5.0f,
                     float current_phase = 0.0f,
                     uint8_t number_of_channels = 4,
                     uint32_t number_of_samples_per_channel = 16384,
                     ssize_t sample_size = 8,
                     const std::string& name = "monopulse_tracker")
      : MonopulseTracker(ArgList{
            Arg("element_spacing", element_spacing),
            Arg("signal_frequency", signal_frequency),
            Arg("phase_step", phase_step),
            Arg("current_phase", current_phase),
            Arg("number_of_channels", number_of_channels),
            Arg("number_of_samples_per_channel", number_of_samples_per_channel),
            Arg("sample_size", sample_size)}) {
    add_positional_condition_and_resource_args(this, args);
    name_ = name;
    fragment_ = fragment;
    spec_ = std::make_shared<OperatorSpec>(fragment);
    setup(*spec_.get());
  }
};

// Pybind11 module definition
PYBIND11_MODULE(_monopulse_tracker, m) {
  m.doc() = R"pbdoc(
    Monopulse Tracker Python Bindings
    ----------------------------------
    .. currentmodule:: _monopulse_tracker
  )pbdoc";

#ifdef VERSION_INFO
  m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
  m.attr("__version__") = "dev";
#endif

  // Bind the MonopulseTracker operator
  py::class_<MonopulseTracker,
             PyMonopulseTracker,
             Operator,
             std::shared_ptr<MonopulseTracker>>(
      m, "MonopulseTracker", doc::MonopulseTracker::doc_MonopulseTracker_python)
      .def(py::init<Fragment*,
                    const py::args&,
                    float,
                    float,
                    float,
                    float,
                    uint8_t,
                    uint32_t,
                    ssize_t,
                    const std::string&>(),
           "fragment"_a,
           "element_spacing"_a,
           "signal_frequency"_a,
           "phase_step"_a = 5.0f,
           "current_phase"_a = 0.0f,
           "number_of_channels"_a = static_cast<uint8_t>(4),
           "number_of_samples_per_channel"_a = static_cast<uint32_t>(16384),
           "sample_size"_a = static_cast<ssize_t>(8),
           "name"_a = "monopulse_tracker"s,
           doc::MonopulseTracker::doc_MonopulseTracker_python)
      .def("initialize",
           &MonopulseTracker::initialize,
           doc::MonopulseTracker::doc_initialize);
}

}  // namespace holoscan::ops
