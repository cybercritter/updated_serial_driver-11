/**
 * @file ring_buffer.c
 * @brief Shared ring-buffer helpers used by the XR17V358 driver model.
 */
#include "ring_buffer.h"

void xr17v358_ring_reset(xr17v358_ring_buffer *rb, size_t capacity) {
  rb->head = 0U;
  rb->tail = 0U;
  rb->size = 0U;
  rb->capacity = capacity;
}

size_t xr17v358_ring_write_bytes(xr17v358_ring_buffer *rb, const uint8_t *data,
                                 size_t length) {
  size_t i = 0U;

  while (i < length && rb->size < rb->capacity) {
    rb->storage[rb->tail] = data[i];
    rb->tail = (rb->tail + 1U) % rb->capacity;
    rb->size++;
    i++;
  }

  return i;
}

size_t xr17v358_ring_read_bytes(xr17v358_ring_buffer *rb, uint8_t *data,
                                size_t length) {
  size_t i = 0U;

  while (i < length && rb->size > 0U) {
    data[i] = rb->storage[rb->head];
    rb->head = (rb->head + 1U) % rb->capacity;
    rb->size--;
    i++;
  }

  return i;
}

size_t xr17v358_ring_frame_count(const xr17v358_ring_buffer *rb) {
  size_t i = 0U;
  size_t frames = 0U;
  size_t payload_bytes = 0U;
  int in_frame = 0;

  while (i < rb->size) {
    const uint8_t value = rb->storage[(rb->head + i) % rb->capacity];

    if (value == XR17V358_FRAME_DELIMITER) {
      if (in_frame && payload_bytes > 0U) {
        frames++;
      }
      in_frame = 1;
      payload_bytes = 0U;
    } else if (in_frame) {
      payload_bytes++;
    }

    i++;
  }

  return frames;
}

xr17v358_error xr17v358_ring_read_frame(xr17v358_ring_buffer *rb,
                                        uint8_t *frame, size_t frame_capacity,
                                        size_t *frame_length) {
  size_t count = 0U;
  uint8_t byte = 0U;

  if (frame == NULL || frame_length == NULL || frame_capacity == 0U) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  while (rb->size > 0U) {
    (void)xr17v358_ring_read_bytes(rb, &byte, 1U);
    if (count == 0U) {
      if (byte != XR17V358_FRAME_DELIMITER) {
        continue;
      }
      frame[count++] = byte;
      continue;
    }

    if (count >= frame_capacity) {
      return XR17V358_ERROR_INVALID_FRAME;
    }

    frame[count++] = byte;
    if (byte == XR17V358_FRAME_DELIMITER) {
      *frame_length = count;
      return XR17V358_OK;
    }
  }

  return XR17V358_ERROR_INVALID_FRAME;
}

xr17v358_error xr17v358_ring_transfer_frame(xr17v358_ring_buffer *source,
                                            xr17v358_ring_buffer *destination) {
  uint8_t frame[XR17V358_MAX_ENCODED_BYTE_COUNT];
  size_t frame_length = 0U;
  size_t written = 0U;
  xr17v358_error error;

  error = xr17v358_ring_read_frame(source, frame, sizeof(frame), &frame_length);
  if (error != XR17V358_OK) {
    return error;
  }

  written = xr17v358_ring_write_bytes(destination, frame, frame_length);
  if (written != frame_length) {
    return XR17V358_ERROR_INVALID_FRAME;
  }

  return XR17V358_OK;
}

int xr17v358_ring_can_accept_frame(const xr17v358_ring_buffer *rb,
                                   size_t max_frame_count) {
  return xr17v358_ring_frame_count(rb) < max_frame_count &&
         rb->size + XR17V358_MAX_ENCODED_BYTE_COUNT <= rb->capacity;
}

xr17v358_error xr17v358_ring_transfer_frames(xr17v358_ring_buffer *source,
                                             xr17v358_ring_buffer *destination,
                                             size_t destination_frame_capacity,
                                             size_t *transferred_count) {
  size_t transferred = 0U;
  xr17v358_error error = XR17V358_OK;

  if (transferred_count == NULL) {
    return XR17V358_ERROR_INVALID_ARGUMENT;
  }

  while (xr17v358_ring_frame_count(source) > 0U &&
         xr17v358_ring_can_accept_frame(destination,
                                        destination_frame_capacity)) {
    error = xr17v358_ring_transfer_frame(source, destination);
    if (error != XR17V358_OK) {
      return error;
    }
    transferred++;
  }

  *transferred_count = transferred;
  return XR17V358_OK;
}
