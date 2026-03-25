/**
 * @file xr17v358.h
 * @brief Production API for the modeled XR17V358 UART driver.
 *
 * @details
 * This header contains the runtime-facing driver surface used by production
 * code. `xr17v358_write()` buffers outbound payload packets, `xr17v358_poll_port()`
 * advances buffered TX messages into the modeled TX FIFO and reports whether a
 * complete RX message is available, and `xr17v358_read()` decodes complete RX
 * messages into an internal read ring before returning payload bytes. Test
 * scaffolding, demo entry points, and FIFO inspection helpers are declared
 * separately in `helpers.h`.
 */
#ifndef XR17V358_H
#define XR17V358_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status and readiness codes returned by XR17V358 driver operations.
 */
typedef enum xr17v358_error {
  /** Polling did not leave a complete RX message ready to read. */
  XR17V358_MESSAGE_NOT_READY = 0,
  /** Generic success status for non-polling operations. */
  XR17V358_OK = XR17V358_MESSAGE_NOT_READY,
  /** Polling left at least one complete RX message ready to read. */
  XR17V358_MESSAGE_READY = 1,
  /** One or more arguments were null, malformed, or otherwise invalid. */
  XR17V358_ERROR_INVALID_ARGUMENT = -1,
  /** The selected UART port index is outside the supported range. */
  XR17V358_ERROR_INVALID_PORT = -2,
  /** The supplied or decoded serial message was invalid. */
  XR17V358_ERROR_INVALID_FRAME = -3,
} xr17v358_error;

/**
 * @brief Supported UART stop-bit selections.
 */
typedef enum xr17v358_stop_bits {
  /** Transmit or receive using one stop bit. */
  XR17V358_STOP_BITS_1 = 1,
  /** Transmit or receive using two stop bits. */
  XR17V358_STOP_BITS_2 = 2,
} xr17v358_stop_bits;

/**
 * @brief Supported UART parity modes.
 */
typedef enum xr17v358_parity {
  /** Disable parity generation and checking. */
  XR17V358_PARITY_NONE = 0,
  /** Enable even parity. */
  XR17V358_PARITY_EVEN = 1,
  /** Enable odd parity. */
  XR17V358_PARITY_ODD = 2,
} xr17v358_parity;

/**
 * @brief Per-port UART configuration.
 */
typedef struct xr17v358_port_config {
  /** Desired baud rate in bits per second. */
  uint32_t baud_rate;
  /** Configured stop-bit mode. */
  xr17v358_stop_bits stop_bits;
  /** Configured parity mode. */
  xr17v358_parity parity;
} xr17v358_port_config;

/** Physical base address of the XR17V358 MMIO region. */
#define XR17V358_DEVICE_BASE_ADDRESS 0x10000000U

/**
 * @brief Initialize one XR17V358 UART port.
 * @param port_index Zero-based UART port number.
 * @param config Desired serial configuration.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_initialize_port(size_t port_index,
                                        const xr17v358_port_config *config);

/**
 * @brief Read back the current configuration for a UART port.
 * @param port_index Zero-based UART port number.
 * @param config Output structure populated on success.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_get_port_config(size_t port_index,
                                        xr17v358_port_config *config);

/**
 * @brief Encode one payload packet using the driver's 0x7E-delimited message format.
 * @param port_index Zero-based UART port number.
 * @param data Payload bytes to encode.
 * @param data_length Number of payload bytes in @p data.
 * @param frame Output buffer for the encoded message.
 * @param frame_capacity Capacity of @p frame in bytes.
 * @return Number of encoded bytes written to @p frame, or zero on failure.
 */
size_t xr17v358_encode_serial_data(size_t port_index, const uint8_t *data,
                                   size_t data_length, uint8_t *frame,
                                   size_t frame_capacity);

/**
 * @brief Decode one encoded serial message into payload bytes.
 * @param port_index Zero-based UART port number.
 * @param frame Encoded message buffer.
 * @param frame_length Number of bytes in @p frame.
 * @param data Output payload buffer.
 * @param data_capacity Capacity of @p data in bytes.
 * @param data_length Output payload length on success.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_decode_serial_data(size_t port_index,
                                           const uint8_t *frame,
                                           size_t frame_length, uint8_t *data,
                                           size_t data_capacity,
                                           size_t *data_length);

/**
 * @brief Return the number of UART ports modeled by the driver.
 * @return Port count for the device.
 */
size_t xr17v358_get_port_count(void);

/**
 * @brief Return the per-port MMIO offsets used by the driver.
 * @return Pointer to a static array of port offsets.
 */
const uint32_t *xr17v358_get_port_offsets(void);

/**
 * @brief Compute the UART base address for a selected port.
 * @param port_index Zero-based UART port number.
 * @param uart_base Output address on success.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_get_uart_base(size_t port_index, uint32_t *uart_base);

/**
 * @brief Compute the TX FIFO window base address for a selected port.
 * @param port_index Zero-based UART port number.
 * @param fifo_base Output FIFO window address on success.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_get_tx_fifo_base(size_t port_index,
                                         uint32_t *fifo_base);

/**
 * @brief Queue one payload packet into the driver's internal TX ring buffer.
 * @param port_index Zero-based UART port number.
 * @param data Payload bytes to buffer for a later poll.
 * @param length Number of bytes available in @p data.
 * @param bytes_written Output count of payload bytes accepted into the ring
 * buffer.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_write(size_t port_index, const uint8_t *data,
                              size_t length, size_t *bytes_written);

/**
 * @brief Read payload bytes from the modeled receive path.
 * @param port_index Zero-based UART port number.
 * @param data Output buffer for decoded payload bytes.
 * @param length Maximum number of bytes to return.
 * @param bytes_read Output count of payload bytes returned.
 * @details
 * When complete encoded RX messages are still waiting in the modeled RX FIFO,
 * this call decodes them into an internal read ring before copying payload bytes
 * into @p data.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_read(size_t port_index, uint8_t *data, size_t length,
                             size_t *bytes_read);

/**
 * @brief Write raw bytes into the memory-mapped TX FIFO window.
 * @param device_base Base address of the XR17V358 MMIO region.
 * @param port_index Zero-based UART port number.
 * @param data Raw bytes to store in the FIFO window.
 * @param length Number of bytes available in @p data.
 * @param bytes_written Output count of bytes written to MMIO.
 * @details
 * The helper packs groups of four bytes into 32-bit FIFO words and writes any
 * trailing bytes individually.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_write_tx_fifo(volatile void *device_base,
                                      size_t port_index, const uint8_t *data,
                                      size_t length, size_t *bytes_written);

/**
 * @brief Read raw bytes from the memory-mapped RX FIFO window.
 * @param device_base Base address of the XR17V358 MMIO region.
 * @param port_index Zero-based UART port number.
 * @param data Output buffer for bytes read from the FIFO window.
 * @param length Maximum number of bytes to read from MMIO.
 * @param bytes_read Output count of bytes read from MMIO.
 * @details
 * The helper reads no more than the reported hardware RX FIFO level and
 * unpacks 32-bit FIFO words into byte order when possible.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_read_rx_fifo(volatile void *device_base,
                                     size_t port_index, uint8_t *data,
                                     size_t length, size_t *bytes_read);

/**
 * @brief Queue one payload packet and flush as much as possible into the
 * hardware TX FIFO.
 * @param device_base Base address of the XR17V358 MMIO region.
 * @param port_index Zero-based UART port number.
 * @param data Payload bytes to encode and transmit.
 * @param length Number of payload bytes available in @p data.
 * @param bytes_written Output count of payload bytes accepted by the driver.
 * @details
 * Accepted payload bytes are encoded into the driver's 0x7E-delimited framing,
 * buffered in the internal TX queue, and then pushed into the hardware FIFO up
 * to the current `TXLVL` space.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_write_hw(volatile void *device_base, size_t port_index,
                                 const uint8_t *data, size_t length,
                                 size_t *bytes_written);

/**
 * @brief Read payload bytes from the hardware RX FIFO path.
 * @param device_base Base address of the XR17V358 MMIO region.
 * @param port_index Zero-based UART port number.
 * @param data Output buffer for decoded payload bytes.
 * @param length Maximum number of payload bytes to return.
 * @param bytes_read Output count of payload bytes returned.
 * @details
 * The driver first pulls raw encoded bytes from the hardware RX FIFO into its
 * internal staging ring, then decodes complete frames into the read queue
 * before copying payload bytes into @p data.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_read_hw(volatile void *device_base, size_t port_index,
                                uint8_t *data, size_t length,
                                size_t *bytes_read);

/**
 * @brief Service the port's modeled TX and RX data paths.
 * @param port_index Zero-based UART port number.
 * @details
 * Polling flushes buffered outbound messages from the internal TX write ring
 * into the modeled TX FIFO. It does not consume RX data, but it does
 * report whether a complete RX message is already available either in the RX
 * FIFO or in the decoded read ring.
 * @return XR17V358_MESSAGE_READY when a complete RX message is available after
 * polling, XR17V358_MESSAGE_NOT_READY when more RX data is still needed, or an
 * error code on failure.
 */
xr17v358_error xr17v358_poll_port(size_t port_index);

/**
 * @brief Service one port using the memory-mapped hardware FIFOs.
 * @param device_base Base address of the XR17V358 MMIO region.
 * @param port_index Zero-based UART port number.
 * @details
 * Polling stages pending TX queue bytes into the hardware TX FIFO up to the
 * current `TXLVL` space and pulls newly available RX bytes out of the hardware
 * RX FIFO according to `RXLVL`.
 * @return XR17V358_MESSAGE_READY when a complete RX message is available after
 * polling, XR17V358_MESSAGE_NOT_READY when more RX data is still needed, or an
 * error code on failure.
 */
xr17v358_error xr17v358_poll_hw_port(volatile void *device_base,
                                     size_t port_index);

/**
 * @brief Assert or deassert the active-low discrete output routed through RTS.
 * @param device_base Base address of the XR17V358 MMIO region.
 * @param port_index Zero-based UART port number.
 * @param asserted Non-zero to assert the active-low output.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_set_discrete(volatile void *device_base,
                                     size_t port_index, int asserted);

/**
 * @brief Enable or disable internal UART loopback through the MCR register.
 * @param device_base Base address of the XR17V358 MMIO region.
 * @param port_index Zero-based UART port number.
 * @param enabled Non-zero to enable loopback, zero to disable it.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_set_loopback(volatile void *device_base,
                                     size_t port_index, int enabled);

#ifdef __cplusplus
}
#endif

#endif
