/**
 * @file ring_buffer.h
 * @brief Internal ring-buffer primitives used by the XR17V358 model.
 *
 * @details
 * This header is shared by the driver core and helper layer. It is kept in the
 * public include tree because the current library target exposes that include
 * directory, but it is an internal implementation detail rather than part of
 * the stable application-facing API.
 */
#ifndef XR17V358_RING_BUFFER_H
#define XR17V358_RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#include "xr17v358.h"

/** Maximum number of payload bytes tracked in each software queue model. */
#define XR17V358_QUEUE_CAPACITY 1024U
/** Maximum number of bytes transferred per modeled FIFO service step. */
#define XR17V358_FIFO_CAPACITY 256U
/** HDLC-style message delimiter used by the packet model. */
#define XR17V358_FRAME_DELIMITER 0x7EU
/** Largest encoded message size for one payload packet. */
#define XR17V358_MAX_ENCODED_BYTE_COUNT (2U + (XR17V358_QUEUE_CAPACITY * 2U))
/** Maximum raw storage reserved for one internal ring buffer. */
#define XR17V358_RING_BUFFER_STORAGE_CAPACITY \
  (XR17V358_FIFO_CAPACITY * XR17V358_MAX_ENCODED_BYTE_COUNT)

/**
 * @brief Internal byte ring buffer used for FIFO and queue modeling.
 */
typedef struct xr17v358_ring_buffer {
  /** Raw storage backing the ring buffer. */
  uint8_t storage[XR17V358_RING_BUFFER_STORAGE_CAPACITY];
  /** Read position inside @ref storage. */
  size_t head;
  /** Write position inside @ref storage. */
  size_t tail;
  /** Number of valid bytes currently stored. */
  size_t size;
  /** Maximum number of bytes allowed in the buffer. */
  size_t capacity;
} xr17v358_ring_buffer;

/**
 * @brief Reset a ring buffer to an empty state.
 * @param rb Ring buffer to reset.
 * @param capacity Maximum number of bytes the buffer may hold.
 */
void xr17v358_ring_reset(xr17v358_ring_buffer *rb, size_t capacity);

/**
 * @brief Append bytes to a ring buffer until it fills.
 * @param rb Destination ring buffer.
 * @param data Source byte buffer.
 * @param length Number of bytes available in @p data.
 * @return Number of bytes accepted into @p rb.
 */
size_t xr17v358_ring_write_bytes(xr17v358_ring_buffer *rb, const uint8_t *data,
                                 size_t length);

/**
 * @brief Remove bytes from a ring buffer.
 * @param rb Source ring buffer.
 * @param data Destination byte buffer.
 * @param length Maximum number of bytes to read.
 * @return Number of bytes removed from @p rb.
 */
size_t xr17v358_ring_read_bytes(xr17v358_ring_buffer *rb, uint8_t *data,
                                size_t length);

/**
 * @brief Count complete 0x7E-delimited messages in a ring buffer.
 * @param rb Source ring buffer.
 * @return Number of complete encoded messages currently buffered.
 */
size_t xr17v358_ring_frame_count(const xr17v358_ring_buffer *rb);

/**
 * @brief Check whether a ring buffer contains a delimited frame terminator.
 * @param rb Source ring buffer.
 * @return Non-zero when bytes from one delimiter through a later delimiter are
 * buffered, otherwise zero.
 */
int xr17v358_ring_has_complete_frame(const xr17v358_ring_buffer *rb);

/**
 * @brief Remove one complete encoded message from a ring buffer.
 * @param rb Source ring buffer.
 * @param frame Output buffer for the encoded message.
 * @param frame_capacity Capacity of @p frame in bytes.
 * @param frame_length Output encoded message length on success.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_ring_read_frame(xr17v358_ring_buffer *rb,
                                        uint8_t *frame, size_t frame_capacity,
                                        size_t *frame_length);

/**
 * @brief Move one complete encoded message between two ring buffers.
 * @param source Source ring buffer.
 * @param destination Destination ring buffer.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_ring_transfer_frame(xr17v358_ring_buffer *source,
                                            xr17v358_ring_buffer *destination);

/**
 * @brief Check whether a destination buffer can accept another complete message.
 * @param rb Destination ring buffer.
 * @param max_frame_count Maximum allowed number of complete messages.
 * @return Non-zero when another message can be accepted, otherwise zero.
 */
int xr17v358_ring_can_accept_frame(const xr17v358_ring_buffer *rb,
                                   size_t max_frame_count);

/**
 * @brief Move as many complete messages as possible between two buffers.
 * @param source Source ring buffer.
 * @param destination Destination ring buffer.
 * @param destination_frame_capacity Maximum allowed number of complete messages.
 * @param transferred_count Output count of messages moved.
 * @return XR17V358_OK on success or an error code on failure.
 */
xr17v358_error xr17v358_ring_transfer_frames(xr17v358_ring_buffer *source,
                                             xr17v358_ring_buffer *destination,
                                             size_t destination_frame_capacity,
                                             size_t *transferred_count);

#endif
