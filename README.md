# Holoscan IIO Controller

A Holoscan Module for interfacing with Industrial I/O (IIO) devices — software-defined
radios (SDRs) like the ADALM-Pluto, data acquisition systems, and sensors exposed through the
Linux IIO subsystem.

Supports both **libiio v0** (the legacy, string-based API) and **libiio v1** (the current,
object-based API) through a single interface, with the correct backend selected automatically
at build time via `pkg-config` — no code changes needed to switch between them.

---

## Quick Start

If libiio is installed normally (system package manager, or a plain source install with no
custom prefix), it's already on `pkg-config`'s default search path — nothing extra is needed:

```bash
holoscan build iio --local
holoscan run iio --local
```

If you have multiple libiio versions installed side-by-side and need to select a specific one,
point `pkg-config` at it explicitly:

```bash
PKG_CONFIG_PATH=/path/to/libiio-v0-or-v1/lib/pkgconfig holoscan build iio --local
```

For Python:

```bash
holoscan run iio --local --language python
```

---

## What's Included

| Operator | Purpose |
| --- | --- |
| `IIOAttributeRead` | Read a single device/channel/context attribute |
| `IIOAttributeWrite` | Write a single device/channel/context attribute |
| `IIOBufferRead` | Stream samples from a device's RX buffer |
| `IIOBufferWrite` | Stream samples to a device's TX buffer |
| `IIOConfigurator` | Apply a full device configuration from a YAML file |

Full parameter reference, port types, and usage examples for each operator:
[`operators/iio_controller/README.md`](operators/iio_controller/README.md).

**Example application** — `applications/iio/`: a `basic_example` (attribute and buffer
read/write demos) and a `pluto_fft_example` (transmit → receive via loopback → FFT → real-time
spectrum plot), both in C++ and Python.

---

## Requirements

- **libiio** ≥ 0.26 (v0) or any v1.x release — auto-detected via `pkg-config`
- **Holoscan SDK** ≥ 4.6.0
- An IIO-capable device for real use (tested against the ADALM-Pluto SDR); attribute-only
  operators can be exercised against any reachable IIO context

---

## Testing

```bash
cd build/iio
ctest --output-on-failure
```

(`holoscan test`'s default container-driver flow expects a shared CTest script this standalone
module doesn't carry; running `ctest` directly against an already-built `build/iio` works and is
what the commands above assume.)

The test suite (`operators/iio_controller/python/tests/`) covers Python import correctness and
operator construction with real parameter names — it doesn't require hardware, since none of it
calls `initialize()`.

---

## A Note on `simple_fft`

`applications/iio/cpp/pluto_fft_example/simple_fft.{hpp,cu}` is a minimal FFT operator derived
from [`nvidia-holoscan/holohub`'s `operators/fft`](https://github.com/nvidia-holoscan/holohub/tree/main/operators/fft)
(© Voyager Technologies, Inc., Apache-2.0), with the VITA 49.2 metadata-passthrough parameters
removed since this example doesn't use them. See the file headers for the full attribution.

---

## License

Apache-2.0 — see [LICENSE](LICENSE). Third-party attributions are listed in [NOTICE](NOTICE)
and in the headers of individual files where applicable.
