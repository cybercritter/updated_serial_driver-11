/**
 * @file xr17v358_internal.h
 * @brief Internal XR17V358 driver declarations shared by core and helper code.
 *
 * @details
 * This header is private to the implementation. It exposes constants, shared
 * state, and internal helper routines used by `xr17v358.c` and
 * `xr17v358_helpers.c`. Generic ring-buffer primitives live in
 * `ring_buffer.h`.
 */
#ifndef XR17V358_INTERNAL_H
#define XR17V358_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ring_buffer.h"
#include "xr17v358.h"
#include "xr17v358_register_map.h"

/** Offset of the memory-mapped FIFO window from each port base. */
#define XR17V358_FIFO_REGISTER_OFFSET 0x0100U
/** Escape byte used by the message encoding model. */
#define XR17V358_FRAME_ESCAPE 0x7DU
/** XOR mask used when escaping reserved bytes. */
#define XR17V358_FRAME_ESCAPE_XOR 0x20U
/** Default baud rate applied during lazy initialization. */
#define XR17V358_DEFAULT_BAUD_RATE 115200U

/** Static port-offset table shared across the implementation. */
extern const uint32_t k_port_offsets[];
/** Modeled TX FIFO state for each port. */
extern xr17v358_ring_buffer tx_fifo[XR17V358_PORT_COUNT];
/** Modeled RX FIFO state for each port. */
extern xr17v358_ring_buffer rx_fifo[XR17V358_PORT_COUNT];
/** Buffered TX messages waiting to be moved into the TX FIFO. */
extern xr17v358_ring_buffer tx_queue[XR17V358_PORT_COUNT];
/** Decoded RX payload bytes waiting to be returned to callers. */
extern xr17v358_ring_buffer rx_queue[XR17V358_PORT_COUNT];
/** Current UART configuration for each port. */
extern xr17v358_port_config port_config[XR17V358_PORT_COUNT];
/** Non-zero once shared state has been initialized. */
extern bool is_initialized;

/**
 * @brief Reset all modeled state, including helper-visible buffers.
 */
void xr17v358_reset(void);

/**
 * @brief Reset shared state without helper-specific buffer scrubbing.
 */
void xr17v358_reset_state(void);

/**
 * @brief Check whether a port index is in range.
 * @param port_index Zero-based UART port number.
 * @return Non-zero when the port is valid, otherwise zero.
 */
int xr17v358_is_valid_port(size_t port_index);

/**
 * @brief Validate a port index for internal callers.
 * @param port_index Zero-based UART port number.
 * @return XR17V358_OK when valid or XR17V358_ERROR_INVALID_PORT otherwise.
 */
xr17v358_error xr17v358_validate_port_index(size_t port_index);

/**
 * @brief Validate a UART configuration structure.
 * @param config Configuration to validate.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_validate_port_config(
    const xr17v358_port_config *config);

/**
 * @brief Return the default UART configuration used during initialization.
 * @return Default per-port configuration.
 */
xr17v358_port_config xr17v358_default_port_config(void);

/**
 * @brief Lazily initialize shared state when needed.
 */
void xr17v358_ensure_state_initialized(void);

/**
 * @brief Inject raw encoded message bytes into one of the helper-managed
 * buffers.
 * @param buffers Target buffer array indexed by port.
 * @param port_index Zero-based UART port number.
 * @param data Encoded bytes to inject.
 * @param length Number of bytes available in @p data.
 * @param bytes_written Output count of injected bytes.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_inject_frame_bytes(xr17v358_ring_buffer *buffers,
                                           size_t port_index,
                                           const uint8_t *data, size_t length,
                                           size_t *bytes_written);

#endif
