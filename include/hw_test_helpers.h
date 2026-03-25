/**
 * @file helpers.h
 * @brief Non-production XR17V358 helper APIs used by tests.
 *
 * @details
 * This header exposes reset, injection, inspection, and queue-drain helpers
 * that are intentionally kept separate from the production driver API. Several
 * names retain older "queue" terminology for compatibility, even though the
 * current implementation inspects the modeled outbound TX FIFO.
 */
#ifndef XR17V358_HELPERS_H
#define XR17V358_HELPERS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "xr17v358.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset the modeled helper state for all ports.
 */
void xr17v358_reset(void);

/**
 * @brief Return the modeled FIFO capacity in complete messages.
 * @return Maximum number of complete encoded messages tracked per FIFO.
 */
size_t xr17v358_get_fifo_capacity(void);

/**
 * @brief Return the modeled software-queue payload limit in bytes.
 * @return Maximum payload size accepted for one queued packet.
 */
size_t xr17v358_get_queue_capacity(void);

/**
 * @brief Return the current RX FIFO fill level for one port.
 * @param port_index Zero-based UART port number.
 * @return Number of complete RX messages buffered for the port, or zero for an
 * invalid port.
 */
size_t xr17v358_fifo_level(size_t port_index);

/**
 * @brief Return the current outbound TX FIFO fill level for one port.
 * @param port_index Zero-based UART port number.
 * @return Number of complete TX messages staged in the modeled FIFO for the
 * port, or zero for an invalid port.
 */
size_t xr17v358_queue_size(size_t port_index);

/**
 * @brief Inject one payload packet into the modeled RX FIFO path.
 * @param port_index Zero-based UART port number.
 * @param data Payload bytes to encode and inject as one message.
 * @param length Number of bytes available in @p data.
 * @param bytes_received Output count of accepted payload bytes.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_receive(size_t port_index, const uint8_t *data,
                                size_t length, size_t *bytes_received);

/**
 * @brief Inject already-encoded message bytes directly into the RX FIFO model.
 * @param port_index Zero-based UART port number.
 * @param data Encoded message bytes to inject.
 * @param length Number of bytes available in @p data.
 * @param bytes_written Output count of injected bytes.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_inject_rx_frame_bytes(size_t port_index,
                                              const uint8_t *data,
                                              size_t length,
                                              size_t *bytes_written);

/**
 * @brief Inject already-encoded message bytes directly into the TX FIFO model.
 * @param port_index Zero-based UART port number.
 * @param data Encoded message bytes to inject.
 * @param length Number of bytes available in @p data.
 * @param bytes_written Output count of injected bytes.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_inject_tx_frame_bytes(size_t port_index,
                                              const uint8_t *data,
                                              size_t length,
                                              size_t *bytes_written);

/**
 * @brief Move one pending TX message from the write ring into the TX FIFO.
 * @param port_index Zero-based UART port number.
 * @details
 * This helper mirrors one unit of the work normally performed by
 * `xr17v358_poll_port()`.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_transfer_next_tx_frame(size_t port_index);

/**
 * @brief Inject already-encoded message bytes directly into the outbound TX FIFO model.
 * @param port_index Zero-based UART port number.
 * @param data Encoded message bytes to inject.
 * @param length Number of bytes available in @p data.
 * @param bytes_written Output count of injected bytes.
 * @details
 * The function name retains historical "queue" wording for compatibility with
 * older tests.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_inject_queue_frame_bytes(size_t port_index,
                                                 const uint8_t *data,
                                                 size_t length,
                                                 size_t *bytes_written);

/**
 * @brief Read decoded payload bytes from the modeled outbound TX FIFO path.
 * @param port_index Zero-based UART port number.
 * @param data Output buffer for decoded payload bytes.
 * @param length Maximum number of bytes to return.
 * @param bytes_read Output count of bytes returned.
 * @details
 * Despite the name, this helper decodes complete messages directly from the
 * modeled outbound TX FIFO so tests can inspect what polling staged for
 * transmission. The caller-provided buffer must be large enough for each
 * complete decoded packet read in one pass.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_queue_read(size_t port_index, uint8_t *data,
                                   size_t length, size_t *bytes_read);

#ifdef __cplusplus
}
#endif

#endif
