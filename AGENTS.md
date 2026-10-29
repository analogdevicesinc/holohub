# AGENTS.md

Guidance for AI agents working on `holoscan-iio-controller`, a Holoscan Module for
Holoscan SDK >= 4.6.0. [README.md](README.md) covers setup and usage, and
[DEVELOPER.md](DEVELOPER.md) covers the layout and build/test commands.

## Operator API

This module uses the operator API available since Holoscan SDK 3.x, **not** the
typed-port API introduced in SDK 5.0:

- Operators derive from `holoscan::Operator`, declare `Parameter<T>` members, and
  bind them in `setup(OperatorSpec&)`.
- `compute(InputContext&, OutputContext&, ExecutionContext&)` returns `void`.
- Applications wire operators together via `Application::compose()` and
  `make_operator<Op>(...)`/`add_flow(...)`.

Migrating to the SDK 5.0 typed-port API is a deliberate, separate future effort,
not something to do incrementally alongside unrelated changes -- it touches every
operator's public shape. See DEVELOPER.md for why.

## Commands

| Task | Command |
| --- | --- |
| Build the example (C++) | `holoscan build iio --local --language cpp` |
| Build the example (Python) | `holoscan build iio --local --language python` |
| Run the example | `holoscan run iio --local` |
| Run the tests | `cd build/iio && ctest --output-on-failure` |
| Lint | `holoscan lint` (`--install-dependencies` first time) |
| List projects / check host | `holoscan list --json`, `holoscan env-check --json` |

- `holoscan test`'s default flow expects a shared CTest driver script this
  standalone module doesn't carry (`utilities/testing/holohub.container.ctest`,
  only present in the full HoloHub monorepo). Run `ctest` directly against
  `build/iio` instead, as shown above.
- libiio (v0 or v1) is required and is **not** bundled -- build it from source
  yourself (<https://github.com/analogdevicesinc/libiio>) and, if you have more
  than one version installed, select one explicitly:
  `PKG_CONFIG_PATH=/path/to/libiio-vX/lib/pkgconfig holoscan build iio --local`.
  CMake caches the pkg-config result -- wipe `build/` before switching versions.
- Python additionally needs `pylibiio` (the real libiio Python bindings, separate
  from this module's own pybind11 bindings) on `PYTHONPATH`, and `libiio.so` on
  `LD_LIBRARY_PATH` if it isn't in a standard location. `pylibiio` hard-requires
  libiio v1.

A change is done when `holoscan lint` and the build/test commands above pass.

## Files that change together

- CLI version: `requirements-cli.txt` and `pyproject.toml`.
- Module version: `CMakeLists.txt`, `pyproject.toml`, and the module's and
  operator's `metadata.json` files.
- SDK version and base image: `Dockerfile`, `pyproject.toml`
  (`[tool.holoscan.base-images]`), and the module's `metadata.json`
  (`holoscan_sdk.minimum_required_version`).
- Operator ports or parameters: the operator's own code, the operator table in
  `operators/iio_controller/README.md`, and the Python test suite
  (`operators/iio_controller/python/tests/`).
- Third-party dependencies: `NOTICE`, which lists each one with its version and
  license, including attribution for derived files (see `simple_fft.{hpp,cu}`).

`cmake/` holds CMake helpers that `holoscan create` copied from the CLI at
scaffold time. They belong to this module, and you can edit them. Upgrading the
CLI does not update them; update them by hand if needed.

## Boundaries

- **Always** preview commands that build, run, test, or package with
  `--dryrun --verbose`, using the options you intend to run. Read-only commands
  (`version`, `list`, `modes`, `env-info`, `env-check`, `status`) need no preview.
- **Always** run `holoscan lint` before committing.
- **Always** sign off commits with `git commit -s` (Developer Certificate of
  Origin).
- **Ask first** before changing the SDK version, base images, or CLI pin, and
  before renaming the module, its namespace, or its operators.
- **Ask first** before running anything that needs `sudo` or changes the host,
  such as `apt install`.
- **Ask first** before editing anything under `operators/iio_controller/cpp/`
  or `operators/iio_controller/python/iio_controller_py.cpp` -- this is the
  module's core implementation.
- Register every new operator's Python bindings in
  `operators/iio_controller/python/iio_controller_py.cpp` and cover it in
  `operators/iio_controller/python/tests/`.
- **Never** store secrets or machine-specific paths in `pyproject.toml` or
  `metadata.json`.
- **Never** pin a dependency on another module to a tag or branch; use a full
  commit SHA.
- **Never** delete `build/` or `install/` directories without asking.
