#include "xr17v358.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "xr17v358_internal.h"

const uint32_t k_port_offsets[] = {
    0x0000U, 0x0400U, 0x0800U, 0x0C00U, 0x1000U, 0x1400U, 0x1800U, 0x1C00U,
};
xr17v358_ring_buffer tx_fifo[8];
xr17v358_ring_buffer rx_fifo[8];
xr17v358_ring_buffer tx_queue[8];
xr17v358_ring_buffer rx_queue[8];
xr17v358_port_config port_config[8];
bool is_initialized = false;
/** @brief Determines if a value requires escaping */
static int xr17v358_requires_escape(uint8_t value) {
  return value == XR17V358_FRAME_DELIMITER || value == XR17V358_FRAME_ESCAPE;
}

int xr17v358_is_valid_port(size_t port_index) {
  return port_index < xr17v358_get_port_count();
}

xr17v358_error xr17v358_validate_port_index(size_t port_index) {
  if (!xr17v358_is_valid_port(port_index)) {
    return XR17V358_ERROR_INVALID_PORT;
  }

  return XR17V358_OK;
}

xr17v358_error xr17v358_validate_port_config(
    const xr17v358_port_config *config) {
  if (config == NULL || config->baud_rate == 0U) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  if (config->stop_bits != XR17V358_STOP_BITS_1 &&
      config->stop_bits != XR17V358_STOP_BITS_2) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  if (config->parity != XR17V358_PARITY_NONE &&
      config->parity != XR17V358_PARITY_EVEN &&
      config->parity != XR17V358_PARITY_ODD) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  return XR17V358_OK;
}

xr17v358_port_config xr17v358_default_port_config(void) {
  xr17v358_port_config config;

  config.baud_rate = XR17V358_DEFAULT_BAUD_RATE;
  config.stop_bits = XR17V358_STOP_BITS_1;
  config.parity = XR17V358_PARITY_NONE;
  return config;
}

void xr17v358_ensure_state_initialized(void) {
  if (!is_initialized) {
    xr17v358_reset_state();
  }
}

void xr17v358_reset_state(void) {
  for (int i = 0; i < xr17v358_get_port_count(); ++i) {
    xr17v358_ring_reset(
        &tx_fifo[i], XR17V358_FIFO_CAPACITY * XR17V358_MAX_ENCODED_BYTE_COUNT);
    xr17v358_ring_reset(
        &rx_fifo[i], XR17V358_FIFO_CAPACITY * XR17V358_MAX_ENCODED_BYTE_COUNT);
    xr17v358_ring_reset(&tx_queue[i], XR17V358_RING_BUFFER_STORAGE_CAPACITY);
    xr17v358_ring_reset(&rx_queue[i], XR17V358_RING_BUFFER_STORAGE_CAPACITY);
    port_config[i] = xr17v358_default_port_config();
  }

  is_initialized = true;
}

xr17v358_error xr17v358_inject_frame_bytes(xr17v358_ring_buffer *buffers,
                                           size_t port_index,
                                           const uint8_t *data, size_t length,
                                           size_t *bytes_written) {
  xr17v358_error error;

  xr17v358_ensure_state_initialized();
  error = xr17v358_validate_port_index(port_index);
  if (error != XR17V358_OK) {
    return error;
  }

  if (bytes_written == NULL || (length > 0U && data == NULL)) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  *bytes_written =
      xr17v358_ring_write_bytes(&buffers[port_index], data, length);
  return XR17V358_OK;
}

xr17v358_error xr17v358_initialize_port(size_t port_index,
                                        const xr17v358_port_config *config) {
  xr17v358_error error;

  xr17v358_ensure_state_initialized();

  error = xr17v358_validate_port_index(port_index);
  if (error != XR17V358_OK) {
    return error;
  }

  error = xr17v358_validate_port_config(config);
  if (error != XR17V358_OK) {
    return error;
  }

  port_config[port_index] = *config;
  return XR17V358_OK;
}

xr17v358_error xr17v358_get_port_config(size_t port_index,
                                        xr17v358_port_config *config) {
  xr17v358_error error;

  xr17v358_ensure_state_initialized();

  error = xr17v358_validate_port_index(port_index);
  if (error != XR17V358_OK) {
    return error;
  }

  if (config == NULL) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  *config = port_config[port_index];
  return XR17V358_OK;
}

size_t xr17v358_encode_serial_data(size_t port_index, const uint8_t *data,
                                   size_t data_length, uint8_t *frame,
                                   size_t frame_capacity) {
  size_t frame_length = 0U;
  size_t i = 0U;

  (void)port_index;

  if (frame == NULL || data == NULL || data_length == 0U ||
      data_length > XR17V358_QUEUE_CAPACITY) {
    return 0U;
  }
  /* Check to ensure frame will fit in the provided capacity */
  for (i = 0U; i < data_length; ++i) {
    frame_length += xr17v358_requires_escape(data[i]) ? 2U : 1U;
  }
  frame_length += 2U;
  if (frame_length > frame_capacity) {
    return 0U;
  }

  /* Encode the data into the frame */
  frame_length = 0U;
  frame[frame_length++] = XR17V358_FRAME_DELIMITER;
  for (i = 0U; i < data_length; ++i) {
    if (xr17v358_requires_escape(data[i])) {
      frame[frame_length++] = XR17V358_FRAME_ESCAPE;
      frame[frame_length++] = (uint8_t)(data[i] ^ XR17V358_FRAME_ESCAPE_XOR);
    } else {
      frame[frame_length++] = data[i];
    }
  }
  frame[frame_length++] = XR17V358_FRAME_DELIMITER;
  return frame_length;
}

xr17v358_error xr17v358_decode_serial_data(size_t port_index,
                                           const uint8_t *frame,
                                           size_t frame_length, uint8_t *data,
                                           size_t data_capacity,
                                           size_t *data_length) {
  size_t decoded_length = 0U;
  size_t i = 0U;

  (void)port_index;

  if (frame == NULL || data == NULL || data_length == NULL) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  /* Check to ensure frame has valid delimiters */
  if (frame_length < 3U || frame[0] != XR17V358_FRAME_DELIMITER ||
      frame[frame_length - 1U] != XR17V358_FRAME_DELIMITER) {
    return XR17V358_ERROR_INVALID_FRAME;
  }

  /* Decode the frame into data */
  for (i = 1U; i + 1U < frame_length; ++i) {
    uint8_t value = frame[i];

    if (value == XR17V358_FRAME_DELIMITER) {
      return XR17V358_ERROR_INVALID_FRAME;
    }

    if (value == XR17V358_FRAME_ESCAPE) {
      if (i + 2U >= frame_length) {
        return XR17V358_ERROR_INVALID_FRAME;
      }

      value = (uint8_t)(frame[++i] ^ XR17V358_FRAME_ESCAPE_XOR);
      if (!xr17v358_requires_escape(value)) {
        return XR17V358_ERROR_INVALID_FRAME;
      }
    }

    if (decoded_length >= XR17V358_QUEUE_CAPACITY) {
      return XR17V358_ERROR_INVALID_FRAME;
    }

    if (decoded_length >= data_capacity) {
      return XR17V358_ERROR_INVALID_ARGUMENT;
    }

    data[decoded_length++] = value;
  }

  /* GCOVR_EXCL_START */
  if (decoded_length == 0U) {
    return XR17V358_ERROR_INVALID_FRAME;
  }
  /* GCOVR_EXCL_STOP */

  *data_length = decoded_length;
  return XR17V358_OK;
}

size_t xr17v358_get_port_count(void) {
  return sizeof(k_port_offsets) / sizeof(k_port_offsets[0]);
}

const uint32_t *xr17v358_get_port_offsets(void) { return k_port_offsets; }

xr17v358_error xr17v358_get_uart_base(size_t port_index, uint32_t *uart_base) {
  const uint32_t device_base = 0x10000000U;
  xr17v358_error error = xr17v358_validate_port_index(port_index);

  if (uart_base == NULL) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  if (error != XR17V358_OK) {
    return error;
  }

  *uart_base = device_base + k_port_offsets[port_index];
  return XR17V358_OK;
}

xr17v358_error xr17v358_get_tx_fifo_base(size_t port_index,
                                         uint32_t *fifo_base) {
  const uint32_t device_base = 0x10000000U;
  xr17v358_error error = xr17v358_validate_port_index(port_index);

  if (fifo_base == NULL) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  if (error != XR17V358_OK) {
    return error;
  }

  *fifo_base =
      device_base + k_port_offsets[port_index] + XR17V358_FIFO_REGISTER_OFFSET;
  return XR17V358_OK;
}

xr17v358_error xr17v358_write(size_t port_index, const uint8_t *data,
                              size_t length, size_t *bytes_written) {
  xr17v358_error error;
  uint8_t frame[XR17V358_MAX_ENCODED_BYTE_COUNT];
  size_t frame_length = 0U;
  size_t written = 0U;

  xr17v358_ensure_state_initialized();
  error = xr17v358_validate_port_index(port_index);
  if (error != XR17V358_OK) {
    return error;
  }

  if (bytes_written == NULL || (length > 0U && data == NULL)) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  *bytes_written = 0U;
  if (length == 0U) {
    return XR17V358_OK;
  }

  frame_length = xr17v358_encode_serial_data(port_index, data, length, frame,
                                             sizeof(frame));
  if (frame_length == 0U || tx_queue[port_index].size + frame_length >
                                tx_queue[port_index].capacity) {
    return XR17V358_OK;
  }

  written =
      xr17v358_ring_write_bytes(&tx_queue[port_index], frame, frame_length);
  if (written != frame_length) {
    return XR17V358_ERROR_INVALID_FRAME;
  }

  *bytes_written = length;
  return XR17V358_OK;
}

xr17v358_error xr17v358_read(size_t port_index, uint8_t *data, size_t length,
                             size_t *bytes_read) {
  uint8_t frame[XR17V358_MAX_ENCODED_BYTE_COUNT];
  uint8_t payload[XR17V358_QUEUE_CAPACITY];
  size_t payload_length = 0U;
  size_t frame_length = 0U;
  xr17v358_error error;

  xr17v358_ensure_state_initialized();
  error = xr17v358_validate_port_index(port_index);
  if (error != XR17V358_OK) {
    return error;
  }

  if (bytes_read == NULL || (length > 0U && data == NULL)) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  /* Decode any complete framed RX bytes into the payload-oriented read ring. */
  while (xr17v358_ring_frame_count(&rx_fifo[port_index]) > 0U &&
         rx_queue[port_index].size < rx_queue[port_index].capacity) {
    error = xr17v358_ring_read_frame(&rx_fifo[port_index], frame, sizeof(frame),
                                     &frame_length);
    if (error != XR17V358_OK) {
      return error;
    }
    error =
        xr17v358_decode_serial_data(port_index, frame, frame_length, payload,
                                    sizeof(payload), &payload_length);
    if (error != XR17V358_OK) {
      return error;
    }

    if (xr17v358_ring_write_bytes(&rx_queue[port_index], payload,
                                  payload_length) != payload_length) {
      return XR17V358_ERROR_INVALID_FRAME;
    }
  }

  if (rx_queue[port_index].size == 0U && rx_fifo[port_index].size > 0U) {
    return XR17V358_ERROR_INVALID_FRAME;
  }

  *bytes_read = xr17v358_ring_read_bytes(&rx_queue[port_index], data, length);
  return XR17V358_OK;
}

xr17v358_error xr17v358_write_tx_fifo(volatile void *device_base,
                                      size_t port_index, const uint8_t *data,
                                      size_t length, size_t *bytes_written) {
  volatile uint8_t *fifo_bytes;
  volatile uint32_t *fifo_words;
  uintptr_t fifo_address;
  size_t count;
  size_t i;
  xr17v358_error error = xr17v358_validate_port_index(port_index);

  if (device_base == NULL || bytes_written == NULL ||
      (length > 0U && data == NULL)) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  if (error != XR17V358_OK) {
    return error;
  }

  count = length;
  if (count > XR17V358_FIFO_CAPACITY) {
    count = XR17V358_FIFO_CAPACITY;
  }

  fifo_address = (uintptr_t)device_base + k_port_offsets[port_index] +
                 XR17V358_FIFO_REGISTER_OFFSET;
  fifo_words = (volatile uint32_t *)fifo_address;
  fifo_bytes = (volatile uint8_t *)fifo_address;

  for (i = 0U; i + sizeof(uint32_t) <= count; i += sizeof(uint32_t)) {
    uint32_t word = (uint32_t)data[i] | ((uint32_t)data[i + 1U] << 8U) |
                    ((uint32_t)data[i + 2U] << 16U) |
                    ((uint32_t)data[i + 3U] << 24U);
    fifo_words[i / sizeof(uint32_t)] = word;
  }

  for (; i < count; ++i) {
    fifo_bytes[i] = data[i];
  }

  *bytes_written = count;
  return XR17V358_OK;
}

xr17v358_error xr17v358_poll_port(size_t port_index) {
  xr17v358_error error;

  xr17v358_ensure_state_initialized();
  error = xr17v358_validate_port_index(port_index);
  if (error != XR17V358_OK) {
    return error;
  }

  /* Polling advances buffered TX frames into the modeled TX FIFO. */
  if (xr17v358_ring_frame_count(&tx_queue[port_index]) > 0U) {
    uint8_t frame[XR17V358_MAX_ENCODED_BYTE_COUNT];
    size_t frame_length = 0U;

    while (xr17v358_ring_frame_count(&tx_queue[port_index]) > 0U &&
           xr17v358_ring_can_accept_frame(&tx_fifo[port_index],
                                          XR17V358_FIFO_CAPACITY)) {
      error = xr17v358_ring_read_frame(&tx_queue[port_index], frame,
                                       sizeof(frame), &frame_length);
      if (error != XR17V358_OK) {
        return error;
      }

      /* GCOVR_EXCL_START */
      if (xr17v358_ring_write_bytes(&tx_fifo[port_index], frame,
                                    frame_length) != frame_length) {
        return XR17V358_ERROR_INVALID_FRAME;
      }
      /* GCOVR_EXCL_STOP */
    }
  }

  if (rx_queue[port_index].size > 0U ||
      xr17v358_ring_frame_count(&rx_fifo[port_index]) > 0U) {
    return XR17V358_MESSAGE_READY;
  }

  return XR17V358_MESSAGE_NOT_READY;
}
