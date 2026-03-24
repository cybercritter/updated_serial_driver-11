#include "hw_test_helpers.h"

#include <stddef.h>
#include <string.h>

#include "xr17v358_internal.h"

void xr17v358_reset(void) {
  xr17v358_reset_state();

  for (size_t i = 0; i < xr17v358_get_port_count(); ++i) {
    memset(tx_fifo[i].storage, 0, sizeof(tx_fifo[i].storage));
    memset(rx_fifo[i].storage, 0, sizeof(rx_fifo[i].storage));
    memset(tx_queue[i].storage, 0, sizeof(tx_queue[i].storage));
    memset(rx_queue[i].storage, 0, sizeof(rx_queue[i].storage));
  }
}

size_t xr17v358_get_fifo_capacity(void) { return XR17V358_FIFO_CAPACITY; }

size_t xr17v358_get_queue_capacity(void) { return XR17V358_QUEUE_CAPACITY; }

size_t xr17v358_fifo_level(size_t port_index) {
  xr17v358_ensure_state_initialized();
  if (!xr17v358_is_valid_port(port_index)) {
    return 0U;
  }

  return xr17v358_ring_frame_count(&rx_fifo[port_index]);
}

size_t xr17v358_queue_size(size_t port_index) {
  xr17v358_ensure_state_initialized();
  if (!xr17v358_is_valid_port(port_index)) {
    return 0U;
  }

  return xr17v358_ring_frame_count(&tx_fifo[port_index]);
}

xr17v358_error xr17v358_receive(size_t port_index, const uint8_t *data,
                                size_t length, size_t *bytes_received) {
  uint8_t frame[XR17V358_MAX_ENCODED_BYTE_COUNT];
  size_t frame_length = 0U;
  size_t written = 0U;
  xr17v358_error error;

  xr17v358_ensure_state_initialized();
  error = xr17v358_validate_port_index(port_index);
  if (error != XR17V358_OK) {
    return error;
  }

  if (bytes_received == NULL || (length > 0U && data == NULL)) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  *bytes_received = 0U;
  if (length == 0U) {
    return XR17V358_OK;
  }

  frame_length = xr17v358_encode_serial_data(port_index, data, length, frame,
                                             sizeof(frame));
  if (frame_length == 0U ||
      xr17v358_ring_frame_count(&rx_fifo[port_index]) >=
          XR17V358_FIFO_CAPACITY ||
      rx_fifo[port_index].size + frame_length > rx_fifo[port_index].capacity) {
    return XR17V358_OK;
  }

  written =
      xr17v358_ring_write_bytes(&rx_fifo[port_index], frame, frame_length);
  /* GCOVR_EXCL_START */
  if (written != frame_length) {
    return XR17V358_ERROR_INVALID_FRAME;
  }
  /* GCOVR_EXCL_STOP */

  *bytes_received = length;
  return XR17V358_OK;
}

xr17v358_error xr17v358_inject_rx_frame_bytes(size_t port_index,
                                              const uint8_t *data,
                                              size_t length,
                                              size_t *bytes_written) {
  return xr17v358_inject_frame_bytes(rx_fifo, port_index, data, length,
                                     bytes_written);
}

xr17v358_error xr17v358_inject_tx_frame_bytes(size_t port_index,
                                              const uint8_t *data,
                                              size_t length,
                                              size_t *bytes_written) {
  return xr17v358_inject_frame_bytes(tx_fifo, port_index, data, length,
                                     bytes_written);
}

xr17v358_error xr17v358_transfer_next_tx_frame(size_t port_index) {
  xr17v358_error error;

  xr17v358_ensure_state_initialized();
  error = xr17v358_validate_port_index(port_index);
  if (error != XR17V358_OK) {
    return error;
  }

  if (xr17v358_ring_frame_count(&tx_queue[port_index]) == 0U) {
    return XR17V358_ERROR_INVALID_FRAME;
  }

  if (!xr17v358_ring_can_accept_frame(&tx_fifo[port_index],
                                      XR17V358_FIFO_CAPACITY)) {
    return XR17V358_ERROR_INVALID_FRAME;
  }

  return xr17v358_ring_transfer_frame(&tx_queue[port_index],
                                      &tx_fifo[port_index]);
}

xr17v358_error xr17v358_inject_queue_frame_bytes(size_t port_index,
                                                 const uint8_t *data,
                                                 size_t length,
                                                 size_t *bytes_written) {
  return xr17v358_inject_frame_bytes(tx_fifo, port_index, data, length,
                                     bytes_written);
}

xr17v358_error xr17v358_queue_read(size_t port_index, uint8_t *data,
                                   size_t length, size_t *bytes_read) {
  size_t count = 0U;
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

  while (count < length && tx_fifo[port_index].size > 0U) {
    if (xr17v358_ring_frame_count(&tx_fifo[port_index]) == 0U) {
      return XR17V358_ERROR_INVALID_FRAME;
    }

    error = xr17v358_ring_read_frame(&tx_fifo[port_index], frame, sizeof(frame),
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
    if (payload_length > length - count) {
      return XR17V358_ERROR_INVALID_ARGUMENT;
    }

    memcpy(&data[count], payload, payload_length);
    count += payload_length;
  }

  *bytes_read = count;
  return XR17V358_OK;
}
