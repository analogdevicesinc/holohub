# iio-controller

A Holoscan Module extending the Holoscan SDK.

A **Holoscan Module** — a self-contained, redistributable library that extends
[Holoscan SDK](https://developer.nvidia.com/holoscan-sdk) 5.0 with reusable operators under the
`holoscan::iio_controller` namespace.

> Holoscan SDK 5.0 is C++-first. This Module ships C++ operators plus Python bindings that expose
> them through the SDK operator registry (`import holoscan_iio_controller`). Wheel packaging returns in a
> later release.

---

## Quick Start

Create and activate a standard Python virtual environment, then install the exact CLI version
pinned by this Module:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install \
  --extra-index-url https://pypi.nvidia.com \
  -r requirements-cli.txt

# Build and run the C++ demo application inside the development container
holoscan run iio_controller_pipeline --language cpp

# Or run the Python example, which calls the C++ operator through the bindings
holoscan run iio_controller_pipeline --language python
```

If you use [uv](https://docs.astral.sh/uv/), no separate environment setup or activation is
needed. Prefix the same commands with `uv run`, for example:

```bash
uv run holoscan run iio_controller_pipeline --language cpp
```

The project configuration installs the development tools without building the Module on the host.

---

## Operators

| Operator | Implementation | Ports | Parameters |
| --- | --- | --- | --- |
| `IioControllerOp` | C++ | `in` → `out` (TODO) | TODO |

### Namespace

- C++: `holoscan::iio_controller`

---

## Usage

Holoscan SDK 5.0 applications follow three phases: **compose** a `holoscan::Graph`, **compile**
it into an `ExecutionPlan`, then **run** the plan. Structural mistakes (unknown ports, type
mismatches, missing clocks) are reported as diagnostics at compile time, before anything runs.

```cpp
#include <iostream>

#include <holoscan/core/compile.hpp>
#include <holoscan/core/graph.hpp>
#include <holoscan/core/run.hpp>
#include <holoscan/time/realtime_clock.hpp>

#include <iio_controller_op/iio_controller_op.hpp>

int main() {
  holoscan::Graph graph{"my-pipeline"};
  const auto op = graph.op<holoscan::iio_controller::IioControllerOp>("iio_controller_op");
  // TODO: add your source/sink operators and connect typed ports:
  // graph.add_flow(source->out, op->in);
  // graph.add_flow(op->out, sink->in);
  graph.set_default_clock(graph.add_clock<holoscan::RealtimeClock>("clock"));

  const holoscan::ExecutionPlan plan = holoscan::compile(graph);
  if (!plan.ok()) {
    std::cerr << plan.json() << '\n';
    return 1;
  }
  return holoscan::run(plan);
}
```

See `applications/iio_controller_pipeline/cpp/` for a complete example that owns
its own termination with `holoscan::run_async`.

---

## Building from Source (without Holoscan CLI)

| Requirement | Version |
| --- | --- |
| Holoscan SDK | ≥ 5.0.0 (`apt install holoscan`, the `holoscan-cuda-13` alias) |
| CUDA Toolkit | 13.x (matches the Holoscan SDK CUDA pin; the dev `Dockerfile` uses `cuda13`) |
| CMake | ≥ 4.0 |
| C++ compiler | C++20-capable |
| GoogleTest | any recent release (tests only) |
| Python | 3.12+ with development headers (Python bindings) |
| Holoscan SDK Python package | importable as `holoscan` (Python bindings and tests) |
| nanobind | 3.0.1 (CMake fetches it from GitHub when it is not installed) |
| pytest | any recent release (Python tests only) |

```bash
cmake -S . -B build -DBUILD_ALL=ON -DIIO_CONTROLLER_BUILD_TESTING=ON
cmake --build build -j$(nproc)
```

The module's `IIO_CONTROLLER_BUILD_TESTING` option defaults to `ON` in a
standalone build. For a build without tests, set `-DIIO_CONTROLLER_BUILD_TESTING:BOOL=OFF`
when configuring. The standard `-DBUILD_TESTING=OFF` does not control this module's tests.
Without an installed GoogleTest, CMake downloads it while configuring the tests.

---

## Testing

```bash
holoscan test
```

Or, without the Holoscan CLI:

```bash
ctest --test-dir build --output-on-failure -L unit
```

The generated GTest suite (`tests/cpp/test_operators.cpp`) covers two layers recommended by the
Holoscan SDK 5.0 testing guide: a compile-plan test that asserts the graph is accepted, and a
whole-graph test driven by a `holoscan::SyntheticClock` so it runs deterministically without
hardware. Extend the test doubles and assertions as your operator grows.

---

## License

Apache-2.0 — see [LICENSE](LICENSE). Third-party software is listed in [NOTICE](NOTICE).
