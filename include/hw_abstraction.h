/**
 * @file hw_abstraction.h
 * @brief Minimal Abaco hardware abstraction API for the XR17V358 driver.
 *
 * @details
 * This interface intentionally exposes only the small application-facing API
 * needed by the project. All other device-specific helpers remain internal to
 * the underlying XR17V358 driver.
 *
 * The current top-level CMake build does not compile `src/hw_abstraction.c`,
 * and that source file still needs alignment with the latest XR17V358 driver
 * type names before it can be treated as production-ready.
 */
#ifndef HW_ABSTRACTION_H
#define HW_ABSTRACTION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error codes returned by the abstraction API.
 */
typedef enum abaco_hw_error {
  /** Polling observed that a complete RX message is ready to be read. */
  ABACO_HW_MESSAGE_READY = 1,
  /** Operation completed successfully. */
  ABACO_HW_OK = 0,
  /** One or more arguments were null, malformed, or otherwise invalid. */
  ABACO_HW_ERROR_INVALID_ARGUMENT = -1,
  /** The requested UART port index is outside the supported device range. */
  ABACO_HW_ERROR_INVALID_PORT = -2,
  /** The provided or decoded serial message did not match the expected format. */
  ABACO_HW_ERROR_INVALID_FRAME = -3,
} abaco_hw_error;

/**
 * @brief Supported UART stop-bit selections.
 */
typedef enum abaco_hw_stop_bits {
  ABACO_HW_STOP_BITS_1 = 1,
  ABACO_HW_STOP_BITS_2 = 2,
} abaco_hw_stop_bits;

/**
 * @brief Supported UART parity selections.
 */
typedef enum abaco_hw_parity {
  ABACO_HW_PARITY_NONE = 0,
  ABACO_HW_PARITY_EVEN = 1,
  ABACO_HW_PARITY_ODD = 2,
} abaco_hw_parity;

/**
 * @brief Per-port UART configuration used by the abstraction layer.
 */
typedef struct abaco_hw_port_config {
  /** Desired line baud rate in bits per second. */
  uint32_t baud_rate;
  /** Stop-bit selection for the port. */
  abaco_hw_stop_bits stop_bits;
  /** Parity mode for the port. */
  abaco_hw_parity parity;
} abaco_hw_port_config;

/**
 * @brief Configure a UART port with the supplied serial settings.
 * @param port_index Zero-based UART port number.
 * @param config Pointer to the desired port configuration.
 * @return ABACO_HW_OK on success or an error code on failure.
 */
abaco_hw_error abaco_hw_initialize_port(size_t port_index,
                                        const abaco_hw_port_config *config);

/**
 * @brief Read payload bytes from the modeled receive path.
 * @param port_index Zero-based UART port number.
 * @param data Output buffer for decoded payload bytes.
 * @param length Maximum number of payload bytes to read.
 * @param bytes_read Output count of bytes returned.
 * @return ABACO_HW_OK on success or an error code on failure.
 */
abaco_hw_error abaco_hw_read(size_t port_index, uint8_t *data, size_t length,
                             size_t *bytes_read);

/**
 * @brief Write payload bytes into the modeled transmit path.
 * @param port_index Zero-based UART port number.
 * @param data Payload buffer to transmit.
 * @param length Number of payload bytes in @p data.
 * @param bytes_written Output count of accepted payload bytes.
 * @details
 * The current XR17V358 implementation buffers payload bytes in a software TX
 * ring. A later poll moves buffered bytes into the modeled TX FIFO.
 * @return ABACO_HW_OK on success or an error code on failure.
 */
abaco_hw_error abaco_hw_write(size_t port_index, const uint8_t *data,
                              size_t length, size_t *bytes_written);

/**
 * @brief Service the internal TX and RX paths for one port.
 * @param port_index Zero-based UART port number.
 * @return ABACO_HW_MESSAGE_READY when a complete RX message is available,
 * ABACO_HW_OK on a normal successful poll, or an error code on failure.
 */
abaco_hw_error abaco_hw_poll_port(size_t port_index);

/**
 * @brief Assert or deassert the project's discrete output using the `#RTS`
 * line.
 * @param port_index Zero-based UART port number.
 * @param asserted Non-zero to assert the active-low discrete line, zero to
 * deassert it.
 * @return ABACO_HW_OK on success or an error code on failure.
 */
abaco_hw_error abaco_hw_set_discrete(size_t port_index, int asserted);

/**
 * @brief Enable or disable internal UART loopback through the MCR register.
 * @param port_index Zero-based UART port number.
 * @param enabled Non-zero to enable loopback, zero to disable it.
 * @return ABACO_HW_OK on success or an error code on failure.
 */
abaco_hw_error abaco_hw_set_loopback(size_t port_index, int enabled);

#ifdef __cplusplus
}
#endif

#endif
