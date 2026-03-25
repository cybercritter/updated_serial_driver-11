# fifoupdates

`fifoupdates` is a small C/C++ project that models an XR17V358 multi-port UART
driver, a helper layer for test workflows, and a set of shared internal
ring-buffer primitives used by the driver model.

## Components

### Production driver

The production API lives in
[`include/xr17v358.h`](include/xr17v358.h) and is implemented by
[`src/xr17v358.c`](src/xr17v358.c).

It provides:

- port configuration
- frame encode/decode helpers
- UART and FIFO address calculation
- buffered transmit and receive data movement
- TX FIFO MMIO writes
- RX FIFO MMIO reads
- hardware FIFO-backed TX/RX helpers
- port polling with RX readiness reporting

Modeled TX/RX flow:

- `xr17v358_write()` encodes each outbound payload packet into one frame and
  buffers it in an internal TX ring
- `xr17v358_poll_port()` moves up to one FIFO's worth of TX bytes into the
  modeled TX FIFO and up to one FIFO's worth of pending RX bytes into the
  modeled RX FIFO, preserving partial frame state across polls
- `xr17v358_read()` decodes complete packet frames from the modeled RX FIFO into an
  internal decoded read ring before returning payload bytes

Hardware FIFO flow:

- `xr17v358_write_hw()` encodes payload bytes, buffers them in the TX staging
  ring, and flushes queued bytes into the hardware FIFO according to `TXLVL`
- `xr17v358_poll_hw_port()` continues draining queued TX bytes into the hardware
  FIFO and stages newly reported RX bytes out of the hardware FIFO according to
  `RXLVL`
- `xr17v358_read_hw()` stages raw RX FIFO bytes into the decode path before
  returning payload bytes

The CMake target for this API is `xr17v358`.

### Helper API

Non-production behavior lives in
[`include/hw_test_helpers.h`](include/hw_test_helpers.h) and is implemented by
[`src/hw_test_helpers.c`](src/hw_test_helpers.c).

This layer contains:

- driver state reset
- RX/TX injection helpers used by tests
- outbound TX FIFO inspection helpers

The CMake target for this API is `hw_test_helpers`.

### Ring-buffer module

Shared ring-buffer primitives live in
[`include/ring_buffer.h`](include/ring_buffer.h) and
[`src/ring_buffer.c`](src/ring_buffer.c).

This module is used internally by the driver and helper layer. It is not meant
to be treated as the stable application-facing API.

### Hardware abstraction

The application-facing abstraction lives in [`hw_abstraction.h`](hw_abstraction.h)
and [`src/hw_abstraction.c`](src/hw_abstraction.c).

This layer now routes its read/write/poll operations through the XR17V358
hardware FIFO path and is built as the `hw_abstraction` target.

## Build

Configure and build with CMake:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
```

If you do not want tests, omit `-DBUILD_TESTING=ON`.

## Test

Run the test suite with:

```sh
ctest --test-dir build --output-on-failure
```

## Coverage

The project includes an optional coverage workflow:

```sh
cmake -S . -B build -DENABLE_COVERAGE=ON -DBUILD_TESTING=ON
cmake --build build --target coverage
```

The coverage workflow requires `gcovr` to be available on `PATH`. The
top-level `coverage` target configures and builds a separate
`build-coverage/` tree before generating the report artifacts there.

## Notes

- RX readiness reported by `xr17v358_poll_port()` means a complete RX frame
  ending with the trailing `0x7E` delimiter is already buffered in the RX FIFO;
  polling does not itself consume RX data.
- The helper names `xr17v358_queue_size()`, `xr17v358_inject_queue_frame_bytes()`,
  and `xr17v358_queue_read()` retain older queue terminology for compatibility,
  but they now inspect the modeled outbound TX FIFO path.
- `third_party/googletest` is vendored for local test builds.
