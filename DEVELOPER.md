# Developer Guide — holoscan-iio-controller

Layout, build, and testing for this module.

---

## Module layout

```text
holoscan-iio-controller/
├── metadata.json                    # Module-level metadata (schema: urn:holoscan:module:v2)
├── CMakeLists.txt                   # Root CMake — orchestrates operators/ and applications/
├── Dockerfile                       # Development container image
├── pyproject.toml                   # Development tooling (CLI pin, ruff)
├── cmake/                           # Shared CMake helpers (add_holohub_operator/application, etc.)
├── operators/
│   └── iio_controller/
│       ├── cpp/                     # IIOApi interface + v0/v1 backends + the 5 operators
│       ├── python/                  # pybind11 bindings + tests/ (pytest, no hardware needed)
│       └── metadata.json            # Operator-level metadata
└── applications/
    └── iio/
        ├── cpp/                     # basic_example + pluto_fft_example (C++)
        └── python/                  # same demos, Python
```

There is deliberately **no top-level `tests/` or `pkg/`** — the scaffold this module started
from generated placeholders for both, targeting an SDK version and operator API this module
doesn't use; they were removed rather than half-adapted. Python tests live alongside the code
they test, under `operators/iio_controller/python/tests/`.

---

## Operator API — this is not the SDK 5.0 typed-port style

This module's 5 operators use the operator API available since Holoscan SDK 3.x:

```cpp
class IIOAttributeRead : public holoscan::Operator {
  Parameter<std::string> ctx_p_;
  void setup(OperatorSpec& spec) override;
  void initialize() override;
  void compute(InputContext&, OutputContext&, ExecutionContext&) override;
};
```

SDK 5.0 introduced a different, incompatible operator model (typed `Input<T>`/`Output<T>` port
members, `compute(ExecutionContext&)` returning `expected<void, Error>`). This module targets
**SDK ≥ 4.6.0** and does not use that newer style. Migrating is a deliberate, separate future
effort — not something to do incrementally alongside other changes, since it touches every
operator's public shape.

---

## Building

```bash
holoscan build iio --local                      # C++
holoscan build iio --local --language python    # Python bindings
```

Without the CLI:

```bash
cmake -S . -B build/iio -DBUILD_ALL=ON
cmake --build build/iio -j"$(nproc)"
```

**libiio version selection:** both v0 and v1 are supported; the correct backend is chosen at
`pkg-config` detection time. If you have multiple versions installed side-by-side (as opposed to
one normal system install), select one explicitly:

```bash
PKG_CONFIG_PATH=/path/to/libiio-vX/lib/pkgconfig holoscan build iio --local
```

CMake caches the pkg-config result — wipe `build/` before switching versions, or the old one
silently sticks:

```bash
rm -rf build
PKG_CONFIG_PATH=/path/to/the-other-version/lib/pkgconfig holoscan build iio --local
```

---

## Running

```bash
holoscan run iio --local                        # pluto_fft_example by default
holoscan run iio --local -- --basic-example -r   # basic_example, specific flag
holoscan run iio --local --language python
```

The Python path additionally needs `pylibiio` (the real libiio Python bindings — a separate
package from this module's own pybind11 bindings) on `PYTHONPATH`, and the actual `libiio.so` on
`LD_LIBRARY_PATH` if it's not in a standard location:

```bash
PYTHONPATH=/path/to/libiio/bindings/python:$PYTHONPATH \
LD_LIBRARY_PATH=/path/to/libiio-vX/lib:$LD_LIBRARY_PATH \
holoscan run iio --local --language python
```

Note: `pylibiio`'s loader (`iio/__init__.py`) hard-requires libiio **v1** — it checks the loaded
`.so`'s version and raises if it isn't 1.x. The Python path cannot be tested against v0.

---

## Testing

```bash
cd build/iio
ctest --output-on-failure
```

`holoscan test`'s default flow invokes a shared container-driver CTest script
(`utilities/testing/holohub.container.ctest`) that only exists in the full HoloHub monorepo, not
in a standalone module like this one — running `ctest` directly against an already-built
`build/iio` is the practical workaround.

The suite (`operators/iio_controller/python/tests/test_iio_controller.py`) checks imports and
operator construction with real, correct parameter names. It deliberately never calls
`initialize()` (construction alone doesn't touch libiio — that only happens when a graph is
actually activated), so it needs no hardware and no network access.

---

## `simple_fft`

`applications/iio/cpp/pluto_fft_example/simple_fft.{hpp,cu}` is a minimal FFT operator, derived
from `nvidia-holoscan/holohub`'s `operators/fft` with the unused VITA 49.2 metadata-passthrough
parameters stripped. It exists so this module has no dependency — vendored or fetched — on code
outside its own tree. See the file headers for full attribution.

---

## Further reading

- [`operators/iio_controller/README.md`](operators/iio_controller/README.md) — per-operator
  parameters, ports, and usage examples
- [HoloHub registry documentation](https://github.com/nvidia-holoscan/holohub)
- [Holoscan SDK documentation](https://docs.nvidia.com/holoscan/sdk-user-guide/introduction/getting-started)
