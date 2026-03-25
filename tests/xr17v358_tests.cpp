#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>
#include <vector>

#include "tests/helpers.h"

extern "C" {
#include "include/hw_test_helpers.h"
#include "include/ring_buffer.h"
#include "include/xr17v358.h"
#include "include/xr17v358_internal.h"
}

namespace {

std::vector<uint8_t> make_raw_frame(size_t payload_length,
                                    uint8_t fill = 0x11U) {
  std::vector<uint8_t> frame(payload_length + 2U, fill);

  frame.front() = XR17V358_FRAME_DELIMITER;
  frame.back() = XR17V358_FRAME_DELIMITER;
  return frame;
}

size_t mmio_offset(size_t port_index, uint32_t register_offset) {
  return static_cast<size_t>(xr17v358_get_port_offsets()[port_index] +
                             register_offset);
}

}  // namespace

TEST(Xr17v358, PortOffsetsAreExposedInOrder) {
  xr17v358_reset();

  constexpr std::array<uint32_t, 8> kExpectedOffsets = {
      0x0000U, 0x0400U, 0x0800U, 0x0C00U, 0x1000U, 0x1400U, 0x1800U, 0x1C00U,
  };

  ASSERT_EQ(xr17v358_get_port_count(), kExpectedOffsets.size());

  const uint32_t *offsets = xr17v358_get_port_offsets();
  ASSERT_NE(offsets, nullptr);

  for (size_t i = 0; i < kExpectedOffsets.size(); ++i) {
    EXPECT_EQ(offsets[i], kExpectedOffsets[i]);
  }
}

TEST(Xr17v358, ComputesUartBaseForValidPorts) {
  xr17v358_reset();

  constexpr uint32_t kDeviceBase = 0x10000000U;

  for (size_t i = 0; i < xr17v358_get_port_count(); ++i) {
    uint32_t uart_base = 0;
    ASSERT_EQ(xr17v358_get_uart_base(i, &uart_base), XR17V358_OK);
    EXPECT_EQ(uart_base, kDeviceBase + xr17v358_get_port_offsets()[i]);
  }
}

TEST(Xr17v358, RejectsInvalidPortIndex) {
  xr17v358_reset();

  uint32_t uart_base = 0;
  EXPECT_EQ(xr17v358_get_uart_base(xr17v358_get_port_count(), &uart_base),
            XR17V358_ERROR_INVALID_PORT);
}

TEST(Xr17v358, RejectsNullOutputPointer) {
  xr17v358_reset();

  EXPECT_EQ(xr17v358_get_uart_base(0, nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
}

TEST(Xr17v358, ComputesTxFifoBaseForValidPorts) {
  xr17v358_reset();

  constexpr uint32_t kDeviceBase = 0x10000000U;

  for (size_t i = 0; i < xr17v358_get_port_count(); ++i) {
    uint32_t fifo_base = 0;
    ASSERT_EQ(xr17v358_get_tx_fifo_base(i, &fifo_base), XR17V358_OK);
    EXPECT_EQ(fifo_base,
              kDeviceBase + xr17v358_get_port_offsets()[i] + 0x0100U);
  }
}

TEST(Xr17v358, InitializesPortConfigurationPerPort) {
  xr17v358_reset();

  const xr17v358_port_config config = {
      57600U,
      XR17V358_STOP_BITS_2,
      XR17V358_PARITY_ODD,
  };
  xr17v358_port_config stored{};

  ASSERT_EQ(xr17v358_initialize_port(3, &config), XR17V358_OK);
  ASSERT_EQ(xr17v358_get_port_config(3, &stored), XR17V358_OK);
  EXPECT_EQ(stored.baud_rate, config.baud_rate);
  EXPECT_EQ(stored.stop_bits, config.stop_bits);
  EXPECT_EQ(stored.parity, config.parity);
}

TEST(Xr17v358, RejectsInvalidPortConfiguration) {
  xr17v358_reset();

  const xr17v358_port_config invalid_baud = {0U, XR17V358_STOP_BITS_1,
                                             XR17V358_PARITY_NONE};
  const xr17v358_port_config invalid_stop = {
      115200U,
      static_cast<xr17v358_stop_bits>(3),
      XR17V358_PARITY_NONE,
  };

  EXPECT_EQ(xr17v358_initialize_port(0, nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_initialize_port(0, &invalid_baud),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_initialize_port(0, &invalid_stop),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_get_port_config(xr17v358_get_port_count(), nullptr),
            XR17V358_ERROR_INVALID_PORT);
}

TEST(Xr17v358, LazilyInitializesDefaultPortConfiguration) {
  xr17v358_port_config config{};

  ASSERT_EQ(xr17v358_get_port_config(0, &config), XR17V358_OK);
  EXPECT_EQ(config.baud_rate, 115200U);
  EXPECT_EQ(config.stop_bits, XR17V358_STOP_BITS_1);
  EXPECT_EQ(config.parity, XR17V358_PARITY_NONE);
  EXPECT_EQ(xr17v358_fifo_level(0), 0U);
}

TEST(Xr17v358, EncodesAndDecodesSerialDataUsing7EFraming) {
  xr17v358_reset();

  constexpr std::array<uint8_t, 3> kPayload = {0xA5U, 0x11U, 0x22U};
  std::array<uint8_t, 8> frame{};
  std::array<uint8_t, 3> decoded{};
  size_t decoded_length = 0U;
  const size_t frame_length = xr17v358_encode_serial_data(
      0, kPayload.data(), kPayload.size(), frame.data(), frame.size());

  ASSERT_EQ(frame_length, 5U);
  EXPECT_EQ(frame[0], 0x7EU);
  EXPECT_EQ(frame[1], kPayload[0]);
  EXPECT_EQ(frame[2], kPayload[1]);
  EXPECT_EQ(frame[3], kPayload[2]);
  EXPECT_EQ(frame[4], 0x7EU);
  ASSERT_EQ(
      xr17v358_decode_serial_data(0, frame.data(), frame_length, decoded.data(),
                                  decoded.size(), &decoded_length),
      XR17V358_OK);
  EXPECT_EQ(decoded_length, kPayload.size());
  EXPECT_EQ(decoded, kPayload);
}

TEST(Xr17v358, EncodesAndDecodesSerialDataUsingEscapedDelimiterFraming) {
  xr17v358_reset();

  constexpr std::array<uint8_t, 3> kPayload = {0x7EU, 0x7DU, 0x20U};
  std::array<uint8_t, 8> frame{};
  std::array<uint8_t, 3> decoded{};
  size_t decoded_length = 0U;
  const size_t frame_length = xr17v358_encode_serial_data(
      1, kPayload.data(), kPayload.size(), frame.data(), frame.size());

  ASSERT_EQ(frame_length, 7U);
  EXPECT_EQ(frame[0], 0x7EU);
  EXPECT_EQ(frame[1], 0x7DU);
  EXPECT_EQ(frame[2], 0x5EU);
  EXPECT_EQ(frame[3], 0x7DU);
  EXPECT_EQ(frame[4], 0x5DU);
  EXPECT_EQ(frame[5], 0x20U);
  EXPECT_EQ(frame[6], 0x7EU);
  ASSERT_EQ(
      xr17v358_decode_serial_data(1, frame.data(), frame_length, decoded.data(),
                                  decoded.size(), &decoded_length),
      XR17V358_OK);
  EXPECT_EQ(decoded_length, kPayload.size());
  EXPECT_EQ(decoded, kPayload);
}

TEST(Xr17v358, RejectsInvalidSerialFrame) {
  xr17v358_reset();

  std::array<uint8_t, 4> decoded{};
  size_t decoded_length = 0U;
  const std::array<uint8_t, 3> invalid_delimiter = {0x7EU, 0x7EU, 0x7EU};
  const std::array<uint8_t, 4> invalid_escape = {0x7EU, 0x7DU, 0x11U, 0x7EU};

  EXPECT_EQ(xr17v358_decode_serial_data(
                0, invalid_delimiter.data(), invalid_delimiter.size(),
                decoded.data(), decoded.size(), &decoded_length),
            XR17V358_ERROR_INVALID_FRAME);
  EXPECT_EQ(xr17v358_decode_serial_data(0, invalid_delimiter.data(),
                                        invalid_delimiter.size(), nullptr,
                                        decoded.size(), &decoded_length),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_decode_serial_data(1, invalid_escape.data(),
                                        invalid_escape.size(), decoded.data(),
                                        decoded.size(), &decoded_length),
            XR17V358_ERROR_INVALID_FRAME);
  EXPECT_EQ(xr17v358_decode_serial_data(1, invalid_escape.data(),
                                        invalid_escape.size(), decoded.data(),
                                        decoded.size(), nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
}

TEST(Xr17v358, RejectsSerialEncodeEdgeCases) {
  std::array<uint8_t, 4> frame{};
  constexpr std::array<uint8_t, 1> kPlainPayload = {0x11U};
  constexpr std::array<uint8_t, 1> kEscapedPayload = {0x7EU};

  EXPECT_EQ(
      xr17v358_encode_serial_data(0, kPlainPayload.data(), kPlainPayload.size(),
                                  nullptr, frame.size()),
      0U);
  EXPECT_EQ(xr17v358_encode_serial_data(0, nullptr, kPlainPayload.size(),
                                        frame.data(), frame.size()),
            0U);
  EXPECT_EQ(xr17v358_encode_serial_data(0, kPlainPayload.data(), 0U,
                                        frame.data(), frame.size()),
            0U);
  EXPECT_EQ(xr17v358_encode_serial_data(0, kPlainPayload.data(),
                                        kPlainPayload.size(), frame.data(), 2U),
            0U);
  EXPECT_EQ(
      xr17v358_encode_serial_data(0, kEscapedPayload.data(),
                                  kEscapedPayload.size(), frame.data(), 3U),
      0U);
}

TEST(Xr17v358, HandlesAdditionalPublicArgumentValidation) {
  xr17v358_reset();

  const xr17v358_port_config invalid_parity = {
      115200U,
      XR17V358_STOP_BITS_1,
      static_cast<xr17v358_parity>(3),
  };
  size_t count = 0U;
  uint8_t byte = 0U;
  uint32_t fifo_base = 0U;

  EXPECT_EQ(
      xr17v358_initialize_port(xr17v358_get_port_count(), &invalid_parity),
      XR17V358_ERROR_INVALID_PORT);
  EXPECT_EQ(xr17v358_initialize_port(0, &invalid_parity),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_get_port_config(0, nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_get_tx_fifo_base(xr17v358_get_port_count(), &fifo_base),
            XR17V358_ERROR_INVALID_PORT);
  EXPECT_EQ(xr17v358_write(0, &byte, 1, nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_write(0, nullptr, 1, &count),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_read(0, &byte, 1, nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_read(0, nullptr, 1, &count),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_receive(0, &byte, 1, nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_receive(0, nullptr, 1, &count),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_queue_read(0, &byte, 1, nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_queue_read(0, nullptr, 1, &count),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_write_tx_fifo(reinterpret_cast<void *>(&byte),
                                   xr17v358_get_port_count(), &byte, 1, &count),
            XR17V358_ERROR_INVALID_PORT);
  EXPECT_EQ(xr17v358_poll_port(xr17v358_get_port_count()),
            XR17V358_ERROR_INVALID_PORT);
  EXPECT_EQ(xr17v358_fifo_level(xr17v358_get_port_count()), 0U);
  EXPECT_EQ(xr17v358_queue_size(xr17v358_get_port_count()), 0U);
}

TEST(Xr17v358, ReadRejectsMalformedInjectedRxFrames) {
  xr17v358_reset();

  const std::array<uint8_t, 2> incomplete = {0x7EU, 0x11U};
  const std::array<uint8_t, 2> empty_frame = {0x7EU, 0x7EU};
  const std::array<uint8_t, 4> bad_escape = {0x7EU, 0x7DU, 0x11U, 0x7EU};
  uint8_t output = 0U;
  size_t count = 0U;

  ASSERT_EQ(xr17v358_inject_rx_frame_bytes(0, incomplete.data(),
                                           incomplete.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_read(0, &output, 1, &count), XR17V358_ERROR_INVALID_FRAME);

  xr17v358_reset();
  ASSERT_EQ(xr17v358_inject_rx_frame_bytes(0, empty_frame.data(),
                                           empty_frame.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_read(0, &output, 1, &count), XR17V358_ERROR_INVALID_FRAME);

  xr17v358_reset();
  ASSERT_EQ(xr17v358_inject_rx_frame_bytes(0, bad_escape.data(),
                                           bad_escape.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_read(0, &output, 1, &count), XR17V358_ERROR_INVALID_FRAME);
}

TEST(Xr17v358, QueueReadRejectsMalformedInjectedFrames) {
  xr17v358_reset();

  const std::array<uint8_t, 2> incomplete = {0x7EU, 0x11U};
  const std::array<uint8_t, 2> empty_frame = {0x7EU, 0x7EU};
  uint8_t output = 0U;
  size_t count = 0U;

  ASSERT_EQ(xr17v358_inject_queue_frame_bytes(0, incomplete.data(),
                                              incomplete.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_queue_read(0, &output, 1, &count), XR17V358_OK);
  EXPECT_EQ(count, 0U);

  xr17v358_reset();
  ASSERT_EQ(xr17v358_inject_queue_frame_bytes(0, empty_frame.data(),
                                              empty_frame.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_queue_read(0, &output, 1, &count),
            XR17V358_ERROR_INVALID_FRAME);
}

TEST(Xr17v358, QueueReadRequiresBufferLargeEnoughForWholePacket) {
  xr17v358_reset();

  const std::array<uint8_t, 4> packet = {0x7EU, 0x11U, 0x22U, 0x7EU};
  uint8_t output = 0U;
  size_t count = 0U;

  ASSERT_EQ(
      xr17v358_inject_tx_frame_bytes(0, packet.data(), packet.size(), &count),
      XR17V358_OK);
  EXPECT_EQ(xr17v358_queue_read(0, &output, 1U, &count),
            XR17V358_ERROR_INVALID_ARGUMENT);
}

TEST(Xr17v358, FifoWriteReadPreservesOrder) {
  xr17v358_reset();

  constexpr std::array<uint8_t, 5> kInput = {0x11U, 0x22U, 0x33U, 0x44U, 0x55U};
  std::array<uint8_t, 5> output{};
  size_t received = 0;
  size_t read_count = 0;

  ASSERT_EQ(xr17v358_receive(2, kInput.data(), kInput.size(), &received),
            XR17V358_OK);
  ASSERT_EQ(received, kInput.size());
  ASSERT_EQ(xr17v358_fifo_level(2), 0U);
  ASSERT_EQ(xr17v358_poll_port(2), XR17V358_MESSAGE_READY);
  ASSERT_EQ(xr17v358_fifo_level(2), 1U);

  ASSERT_EQ(xr17v358_read(2, output.data(), output.size(), &read_count),
            XR17V358_OK);
  ASSERT_EQ(read_count, kInput.size());
  EXPECT_EQ(output, kInput);
  EXPECT_EQ(xr17v358_fifo_level(2), 0U);
}

TEST(Xr17v358, ReadPathIsIndependentFromWritePath) {
  xr17v358_reset();

  constexpr std::array<uint8_t, 3> kTxData = {0x11U, 0x22U, 0x33U};
  constexpr std::array<uint8_t, 3> kRxData = {0x44U, 0x55U, 0x66U};
  std::array<uint8_t, 3> output{};
  size_t written = 0U;
  size_t received = 0U;
  size_t read_count = 0U;

  ASSERT_EQ(xr17v358_write(0, kTxData.data(), kTxData.size(), &written),
            XR17V358_OK);
  ASSERT_EQ(written, kTxData.size());
  EXPECT_EQ(xr17v358_fifo_level(0), 0U);

  ASSERT_EQ(xr17v358_receive(0, kRxData.data(), kRxData.size(), &received),
            XR17V358_OK);
  ASSERT_EQ(received, kRxData.size());
  EXPECT_EQ(xr17v358_fifo_level(0), 0U);
  ASSERT_EQ(xr17v358_poll_port(0), XR17V358_MESSAGE_READY);
  EXPECT_EQ(xr17v358_fifo_level(0), 1U);

  ASSERT_EQ(xr17v358_read(0, output.data(), output.size(), &read_count),
            XR17V358_OK);
  ASSERT_EQ(read_count, kRxData.size());
  EXPECT_EQ(output, kRxData);
}

TEST(Xr17v358, ReadStagesRxFifoDataIntoReadBuffer) {
  xr17v358_reset();

  constexpr std::array<uint8_t, 4> kRxData = {0x41U, 0x42U, 0x43U, 0x44U};
  std::array<uint8_t, 2> first_half{};
  std::array<uint8_t, 2> second_half{};
  size_t received = 0U;
  size_t read_count = 0U;
  size_t second_read_count = 0U;

  ASSERT_EQ(xr17v358_receive(3, kRxData.data(), kRxData.size(), &received),
            XR17V358_OK);
  ASSERT_EQ(received, kRxData.size());
  ASSERT_EQ(xr17v358_fifo_level(3), 0U);

  ASSERT_EQ(xr17v358_poll_port(3), XR17V358_MESSAGE_READY);
  EXPECT_EQ(xr17v358_fifo_level(3), 1U);

  ASSERT_EQ(xr17v358_read(3, first_half.data(), first_half.size(), &read_count),
            XR17V358_OK);
  ASSERT_EQ(read_count, first_half.size());
  EXPECT_EQ(first_half[0], kRxData[0]);
  EXPECT_EQ(first_half[1], kRxData[1]);
  EXPECT_EQ(xr17v358_fifo_level(3), 0U);

  ASSERT_EQ(xr17v358_read(3, second_half.data(), second_half.size(),
                          &second_read_count),
            XR17V358_OK);
  ASSERT_EQ(second_read_count, second_half.size());
  EXPECT_EQ(second_half[0], kRxData[2]);
  EXPECT_EQ(second_half[1], kRxData[3]);
}

TEST(Xr17v358, PollerMovesPendingWritesIntoTxFifoUntilItFills) {
  xr17v358_reset();

  const size_t fifo_capacity = xr17v358_get_fifo_capacity();
  std::vector<uint8_t> tx =
      test_helpers::make_incrementing_bytes(fifo_capacity + 20U);
  std::vector<uint8_t> drained;
  size_t polls = 0U;

  for (size_t i = 0; i < tx.size(); ++i) {
    size_t tx_written = 0U;

    ASSERT_EQ(xr17v358_write(1, &tx[i], 1U, &tx_written), XR17V358_OK);
    EXPECT_EQ(tx_written, 1U);
  }

  while (drained.size() < tx.size()) {
    std::vector<uint8_t> batch(fifo_capacity);
    size_t popped = 0U;

    ASSERT_EQ(xr17v358_poll_port(1), XR17V358_MESSAGE_NOT_READY);
    ASSERT_EQ(xr17v358_queue_read(1, batch.data(), batch.size(), &popped),
              XR17V358_OK);
    drained.insert(drained.end(), batch.begin(), batch.begin() + popped);
    polls++;
  }

  EXPECT_GT(polls, 1U);
  EXPECT_EQ(drained, tx);
  EXPECT_EQ(xr17v358_queue_size(1), 0U);
}

TEST(Xr17v358, PollerStagesEachWriteCallAsOneFrame) {
  xr17v358_reset();

  constexpr std::array<uint8_t, 3> kPayload = {0x11U, 0x7EU, 0x22U};
  std::array<uint8_t, 3> drained{};
  size_t written = 0U;
  size_t read_count = 0U;

  ASSERT_EQ(xr17v358_write(2, kPayload.data(), kPayload.size(), &written),
            XR17V358_OK);
  ASSERT_EQ(written, kPayload.size());

  ASSERT_EQ(xr17v358_poll_port(2), XR17V358_MESSAGE_NOT_READY);
  EXPECT_EQ(xr17v358_queue_size(2), 1U);

  ASSERT_EQ(xr17v358_queue_read(2, drained.data(), drained.size(), &read_count),
            XR17V358_OK);
  EXPECT_EQ(read_count, kPayload.size());
  EXPECT_EQ(drained, kPayload);
  EXPECT_EQ(xr17v358_queue_size(2), 0U);
}

TEST(Xr17v358, PollerReturnsNotReadyUntilRxMessageCompletes) {
  xr17v358_reset();

  const std::array<uint8_t, 2> partial_frame = {0x7EU, 0x41U};
  const std::array<uint8_t, 1> frame_end = {0x7EU};
  uint8_t output = 0U;
  size_t count = 0U;

  ASSERT_EQ(xr17v358_inject_rx_frame_bytes(0, partial_frame.data(),
                                           partial_frame.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_poll_port(0), XR17V358_MESSAGE_NOT_READY);

  ASSERT_EQ(xr17v358_inject_rx_frame_bytes(0, frame_end.data(),
                                           frame_end.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_poll_port(0), XR17V358_MESSAGE_READY);

  ASSERT_EQ(xr17v358_read(0, &output, 1U, &count), XR17V358_OK);
  EXPECT_EQ(count, 1U);
  EXPECT_EQ(output, 0x41U);
}

TEST(Xr17v358, WritesPackedDataToTxFifoRegisterWindow) {
  xr17v358_reset();

  std::array<uint8_t, 0x2000> mmio{};
  constexpr std::array<uint8_t, 6> kPayload = {0x10U, 0x21U, 0x32U,
                                               0x43U, 0x54U, 0x65U};
  size_t written = 0;

  ASSERT_EQ(xr17v358_write_tx_fifo(mmio.data(), 2, kPayload.data(),
                                   kPayload.size(), &written),
            XR17V358_OK);
  ASSERT_EQ(written, kPayload.size());

  const auto fifo_offset =
      static_cast<size_t>(xr17v358_get_port_offsets()[2] + 0x0100U);
  EXPECT_EQ(mmio[fifo_offset + 0], 0x10U);
  EXPECT_EQ(mmio[fifo_offset + 1], 0x21U);
  EXPECT_EQ(mmio[fifo_offset + 2], 0x32U);
  EXPECT_EQ(mmio[fifo_offset + 3], 0x43U);
  EXPECT_EQ(mmio[fifo_offset + 4], 0x54U);
  EXPECT_EQ(mmio[fifo_offset + 5], 0x65U);
}

TEST(Xr17v358, TxFifoWriteCapsAtHardwareFifoCapacity) {
  xr17v358_reset();

  std::array<uint8_t, 0x2000> mmio{};
  std::vector<uint8_t> payload(xr17v358_get_fifo_capacity() + 9U, 0xA5U);
  size_t written = 0;

  ASSERT_EQ(xr17v358_write_tx_fifo(mmio.data(), 0, payload.data(),
                                   payload.size(), &written),
            XR17V358_OK);
  EXPECT_EQ(written, xr17v358_get_fifo_capacity());
}

TEST(Xr17v358, ReadsRawBytesFromRxFifoRegisterWindow) {
  xr17v358_reset();

  std::array<uint8_t, 0x2000> mmio{};
  constexpr std::array<uint8_t, 6> kFrameBytes = {
      0x7EU,
      0x10U,
      0x21U,
      0x32U,
      0x43U,
      0x7EU,
  };
  std::array<uint8_t, 8> output{};
  size_t count = 0U;

  const size_t fifo_offset = mmio_offset(2, XR17V358_REG_FIFO);
  const size_t rxlvl_offset = mmio_offset(2, XR17V358_REG_RXLVL);

  mmio[rxlvl_offset] = static_cast<uint8_t>(kFrameBytes.size());
  for (size_t i = 0; i < kFrameBytes.size(); ++i) {
    mmio[fifo_offset + i] = kFrameBytes[i];
  }

  ASSERT_EQ(xr17v358_read_rx_fifo(mmio.data(), 2, output.data(), output.size(),
                                  &count),
            XR17V358_OK);
  EXPECT_EQ(count, kFrameBytes.size());

  for (size_t i = 0; i < kFrameBytes.size(); ++i) {
    EXPECT_EQ(output[i], kFrameBytes[i]);
  }
}

TEST(Xr17v358, HardwareWriteFlushesQueuedBytesIntoTxFifoWindow) {
  xr17v358_reset();

  std::array<uint8_t, 0x2000> mmio{};
  constexpr std::array<uint8_t, 3> kPayload = {0x11U, 0x22U, 0x33U};
  size_t written = 0U;

  const size_t txlvl_offset = mmio_offset(1, XR17V358_REG_TXLVL);
  const size_t fifo_offset = mmio_offset(1, XR17V358_REG_FIFO);
  mmio[txlvl_offset] = 4U;

  ASSERT_EQ(xr17v358_write_hw(mmio.data(), 1, kPayload.data(), kPayload.size(),
                              &written),
            XR17V358_OK);
  EXPECT_EQ(written, kPayload.size());
  EXPECT_EQ(tx_queue[1].size, 1U);
  EXPECT_EQ(mmio[fifo_offset + 0U], XR17V358_FRAME_DELIMITER);
  EXPECT_EQ(mmio[fifo_offset + 1U], kPayload[0]);
  EXPECT_EQ(mmio[fifo_offset + 2U], kPayload[1]);
  EXPECT_EQ(mmio[fifo_offset + 3U], kPayload[2]);
}

TEST(Xr17v358, HardwarePollContinuesFlushingQueuedTxBytes) {
  xr17v358_reset();

  std::array<uint8_t, 0x2000> mmio{};
  constexpr std::array<uint8_t, 3> kPayload = {0x11U, 0x22U, 0x33U};
  size_t written = 0U;
  const size_t txlvl_offset = mmio_offset(0, XR17V358_REG_TXLVL);

  mmio[txlvl_offset] = 2U;
  ASSERT_EQ(xr17v358_write_hw(mmio.data(), 0, kPayload.data(), kPayload.size(),
                              &written),
            XR17V358_OK);
  ASSERT_EQ(written, kPayload.size());
  ASSERT_EQ(tx_queue[0].size, 3U);

  mmio[txlvl_offset] = 1U;
  EXPECT_EQ(xr17v358_poll_hw_port(mmio.data(), 0), XR17V358_MESSAGE_NOT_READY);
  EXPECT_EQ(tx_queue[0].size, 2U);
}

TEST(Xr17v358, HardwareReadDecodesBytesPulledFromRxFifoWindow) {
  xr17v358_reset();

  std::array<uint8_t, 0x2000> mmio{};
  constexpr std::array<uint8_t, 2> kPayload = {0x44U, 0x55U};
  constexpr std::array<uint8_t, 4> kFrame = {
      XR17V358_FRAME_DELIMITER,
      kPayload[0],
      kPayload[1],
      XR17V358_FRAME_DELIMITER,
  };
  std::array<uint8_t, 2> output{};
  size_t count = 0U;

  const size_t fifo_offset = mmio_offset(3, XR17V358_REG_FIFO);
  const size_t rxlvl_offset = mmio_offset(3, XR17V358_REG_RXLVL);
  mmio[rxlvl_offset] = static_cast<uint8_t>(kFrame.size());
  for (size_t i = 0; i < kFrame.size(); ++i) {
    mmio[fifo_offset + i] = kFrame[i];
  }

  ASSERT_EQ(
      xr17v358_read_hw(mmio.data(), 3, output.data(), output.size(), &count),
      XR17V358_OK);
  EXPECT_EQ(count, kPayload.size());
  EXPECT_EQ(output, kPayload);
  EXPECT_EQ(rx_fifo[3].size, 0U);
}

TEST(Xr17v358, HardwarePollReportsPartialRxFrameAsNotReady) {
  xr17v358_reset();

  std::array<uint8_t, 0x2000> mmio{};
  constexpr std::array<uint8_t, 2> kPartialFrame = {
      XR17V358_FRAME_DELIMITER,
      0x41U,
  };

  const size_t fifo_offset = mmio_offset(4, XR17V358_REG_FIFO);
  const size_t rxlvl_offset = mmio_offset(4, XR17V358_REG_RXLVL);
  mmio[rxlvl_offset] = static_cast<uint8_t>(kPartialFrame.size());
  for (size_t i = 0; i < kPartialFrame.size(); ++i) {
    mmio[fifo_offset + i] = kPartialFrame[i];
  }

  EXPECT_EQ(xr17v358_poll_hw_port(mmio.data(), 4), XR17V358_MESSAGE_NOT_READY);
  EXPECT_EQ(rx_fifo[4].size, kPartialFrame.size());
  EXPECT_EQ(xr17v358_ring_frame_count(&rx_fifo[4]), 0U);
}

TEST(Xr17v358, HardwareTransmitMoves1000BytesAcrossMultipleFifoFlushes) {
  xr17v358_reset();

  std::array<uint8_t, 0x2000> mmio{};
  const std::vector<uint8_t> payload =
      test_helpers::make_incrementing_bytes(1000U);
  const size_t hw_fifo_window = std::numeric_limits<uint8_t>::max();
  std::vector<uint8_t> encoded(XR17V358_MAX_ENCODED_BYTE_COUNT);
  std::vector<uint8_t> transmitted;
  std::vector<uint8_t> decoded(payload.size());
  size_t decoded_length = 0U;
  size_t written = 0U;
  const size_t frame_length =
      xr17v358_encode_serial_data(5, payload.data(), payload.size(),
                                  encoded.data(), encoded.size());
  const size_t txlvl_offset = mmio_offset(5, XR17V358_REG_TXLVL);
  const size_t fifo_offset = mmio_offset(5, XR17V358_REG_FIFO);

  ASSERT_GT(frame_length, 0U);
  encoded.resize(frame_length);
  transmitted.reserve(frame_length);

  mmio[txlvl_offset] = static_cast<uint8_t>(hw_fifo_window);
  ASSERT_EQ(xr17v358_write_hw(mmio.data(), 5, payload.data(), payload.size(),
                              &written),
            XR17V358_OK);
  ASSERT_EQ(written, payload.size());

  transmitted.insert(transmitted.end(), mmio.begin() + fifo_offset,
                     mmio.begin() + fifo_offset + (frame_length - tx_queue[5].size));

  while (tx_queue[5].size > 0U) {
    const size_t queued_before = tx_queue[5].size;
    mmio[txlvl_offset] = static_cast<uint8_t>(hw_fifo_window);
    const xr17v358_error poll_result =
        xr17v358_poll_hw_port(mmio.data(), 5);
    const size_t flushed = queued_before - tx_queue[5].size;

    ASSERT_EQ(poll_result, XR17V358_MESSAGE_NOT_READY);
    ASSERT_GT(flushed, 0U);
    transmitted.insert(transmitted.end(), mmio.begin() + fifo_offset,
                       mmio.begin() + fifo_offset + flushed);
  }

  ASSERT_EQ(transmitted, encoded);
  ASSERT_EQ(xr17v358_decode_serial_data(5, transmitted.data(),
                                        transmitted.size(), decoded.data(),
                                        decoded.size(), &decoded_length),
            XR17V358_OK);
  EXPECT_EQ(decoded_length, payload.size());
  EXPECT_EQ(decoded, payload);
}

TEST(Xr17v358, HardwareReceiveMoves1000BytesAcrossMultipleFifoPolls) {
  xr17v358_reset();

  std::array<uint8_t, 0x2000> mmio{};
  const std::vector<uint8_t> payload =
      test_helpers::make_incrementing_bytes(1000U);
  const size_t hw_fifo_window = std::numeric_limits<uint8_t>::max();
  std::vector<uint8_t> encoded(XR17V358_MAX_ENCODED_BYTE_COUNT);
  std::vector<uint8_t> output(payload.size());
  size_t read_count = 0U;
  size_t offset = 0U;
  size_t poll_count = 0U;
  const size_t frame_length =
      xr17v358_encode_serial_data(6, payload.data(), payload.size(),
                                  encoded.data(), encoded.size());
  const size_t fifo_offset = mmio_offset(6, XR17V358_REG_FIFO);
  const size_t rxlvl_offset = mmio_offset(6, XR17V358_REG_RXLVL);

  ASSERT_GT(frame_length, 0U);
  encoded.resize(frame_length);

  while (offset < encoded.size()) {
    const size_t chunk_length =
        std::min(hw_fifo_window, encoded.size() - offset);
    const xr17v358_error expected_result =
        (offset + chunk_length < encoded.size()) ? XR17V358_MESSAGE_NOT_READY
                                                 : XR17V358_MESSAGE_READY;

    mmio[rxlvl_offset] = static_cast<uint8_t>(chunk_length);
    for (size_t i = 0U; i < chunk_length; ++i) {
      mmio[fifo_offset + i] = encoded[offset + i];
    }

    ASSERT_EQ(xr17v358_poll_hw_port(mmio.data(), 6), expected_result);
    offset += chunk_length;
    ++poll_count;
  }

  mmio[rxlvl_offset] = 0U;
  ASSERT_EQ(xr17v358_read_hw(mmio.data(), 6, output.data(), output.size(),
                             &read_count),
            XR17V358_OK);
  EXPECT_EQ(read_count, payload.size());
  EXPECT_EQ(output, payload);
  EXPECT_EQ(rx_fifo[6].size, 0U);
  EXPECT_GT(poll_count, 1U);
}

TEST(Xr17v358, PollerAndApisRejectInvalidArguments) {
  xr17v358_reset();

  size_t count = 0;
  uint8_t byte = 0;

  EXPECT_EQ(xr17v358_write(xr17v358_get_port_count(), &byte, 1, &count),
            XR17V358_ERROR_INVALID_PORT);
  EXPECT_EQ(xr17v358_receive(xr17v358_get_port_count(), &byte, 1, &count),
            XR17V358_ERROR_INVALID_PORT);
  EXPECT_EQ(xr17v358_read(xr17v358_get_port_count(), &byte, 1, &count),
            XR17V358_ERROR_INVALID_PORT);
  EXPECT_EQ(xr17v358_queue_read(xr17v358_get_port_count(), &byte, 1, &count),
            XR17V358_ERROR_INVALID_PORT);
  EXPECT_EQ(xr17v358_get_tx_fifo_base(xr17v358_get_port_count(), nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_write_tx_fifo(nullptr, 0, &byte, 1, &count),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_read_rx_fifo(nullptr, 0, &byte, 1, &count),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_write_hw(nullptr, 0, &byte, 1, &count),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_read_hw(nullptr, 0, &byte, 1, &count),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_poll_hw_port(nullptr, 0),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_poll_port(xr17v358_get_port_count()),
            XR17V358_ERROR_INVALID_PORT);
}

TEST(Xr17v358, RingByteHelpersHandleFullAndEmptyBuffers) {
  xr17v358_ring_buffer rb{};
  std::array<uint8_t, 4> input = {0x10U, 0x21U, 0x32U, 0x43U};
  std::array<uint8_t, 4> output{};

  xr17v358_ring_reset(&rb, 3U);

  EXPECT_EQ(xr17v358_ring_write_bytes(&rb, input.data(), 0U), 0U);
  EXPECT_EQ(xr17v358_ring_write_bytes(&rb, input.data(), input.size()), 3U);
  EXPECT_EQ(xr17v358_ring_write_bytes(&rb, input.data(), 1U), 0U);

  EXPECT_EQ(xr17v358_ring_read_bytes(&rb, output.data(), output.size()), 3U);
  EXPECT_EQ(output[0], input[0]);
  EXPECT_EQ(output[1], input[1]);
  EXPECT_EQ(output[2], input[2]);
  EXPECT_EQ(xr17v358_ring_read_bytes(&rb, output.data(), 1U), 0U);
}

TEST(Xr17v358, RingFrameHelpersHandleNoiseTruncationAndSmallBuffers) {
  xr17v358_ring_buffer rb{};
  std::array<uint8_t, 7> noisy_bytes = {
      0x10U,
      0x21U,
      XR17V358_FRAME_DELIMITER,
      0x33U,
      XR17V358_FRAME_DELIMITER,
      XR17V358_FRAME_DELIMITER,
      0x44U,
  };
  std::array<uint8_t, 4> frame{};
  std::array<uint8_t, 4> large_frame = {
      XR17V358_FRAME_DELIMITER,
      0x11U,
      0x22U,
      XR17V358_FRAME_DELIMITER,
  };
  size_t frame_length = 0U;

  xr17v358_ring_reset(&rb, 16U);
  ASSERT_EQ(
      xr17v358_ring_write_bytes(&rb, noisy_bytes.data(), noisy_bytes.size()),
      noisy_bytes.size());
  EXPECT_EQ(xr17v358_ring_frame_count(&rb), 1U);

  EXPECT_EQ(xr17v358_ring_read_frame(&rb, nullptr, frame.size(), &frame_length),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_ring_read_frame(&rb, frame.data(), 0U, &frame_length),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_ring_read_frame(&rb, frame.data(), frame.size(), nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);

  ASSERT_EQ(
      xr17v358_ring_read_frame(&rb, frame.data(), frame.size(), &frame_length),
      XR17V358_OK);
  EXPECT_EQ(frame_length, 3U);
  EXPECT_EQ(frame[0], XR17V358_FRAME_DELIMITER);
  EXPECT_EQ(frame[1], 0x33U);
  EXPECT_EQ(frame[2], XR17V358_FRAME_DELIMITER);
  EXPECT_EQ(
      xr17v358_ring_read_frame(&rb, frame.data(), frame.size(), &frame_length),
      XR17V358_ERROR_INVALID_FRAME);

  xr17v358_ring_reset(&rb, 16U);
  ASSERT_EQ(
      xr17v358_ring_write_bytes(&rb, large_frame.data(), large_frame.size()),
      large_frame.size());
  EXPECT_EQ(xr17v358_ring_read_frame(&rb, frame.data(), 2U, &frame_length),
            XR17V358_ERROR_INVALID_FRAME);
}

TEST(Xr17v358, RingCanAcceptFrameChecksFrameAndByteLimits) {
  xr17v358_ring_buffer rb{};
  std::array<uint8_t, 3> frame = {
      XR17V358_FRAME_DELIMITER,
      0x5AU,
      XR17V358_FRAME_DELIMITER,
  };

  xr17v358_ring_reset(&rb, XR17V358_MAX_ENCODED_BYTE_COUNT);
  EXPECT_TRUE(xr17v358_ring_can_accept_frame(&rb, 1U));
  ASSERT_EQ(xr17v358_ring_write_bytes(&rb, frame.data(), frame.size()),
            frame.size());
  EXPECT_FALSE(xr17v358_ring_can_accept_frame(&rb, 1U));

  xr17v358_ring_reset(&rb, XR17V358_MAX_ENCODED_BYTE_COUNT - 1U);
  EXPECT_FALSE(xr17v358_ring_can_accept_frame(&rb, XR17V358_FIFO_CAPACITY));
}

TEST(Xr17v358, RingTransferHelpersCoverSuccessAndFailurePaths) {
  xr17v358_ring_buffer source{};
  xr17v358_ring_buffer destination{};
  xr17v358_ring_buffer multi_source{};
  xr17v358_ring_buffer multi_destination{};
  std::array<uint8_t, 3> frame = {
      XR17V358_FRAME_DELIMITER,
      0x5AU,
      XR17V358_FRAME_DELIMITER,
  };
  std::array<uint8_t, 6> two_frames = {
      XR17V358_FRAME_DELIMITER, 0x10U, XR17V358_FRAME_DELIMITER,
      XR17V358_FRAME_DELIMITER, 0x20U, XR17V358_FRAME_DELIMITER,
  };
  const std::vector<uint8_t> oversized_frame =
      make_raw_frame(XR17V358_MAX_ENCODED_BYTE_COUNT);
  size_t transferred = 0U;

  xr17v358_ring_reset(&source, 16U);
  xr17v358_ring_reset(&destination, 16U);
  ASSERT_EQ(xr17v358_ring_write_bytes(&source, frame.data(), frame.size()),
            frame.size());
  EXPECT_EQ(xr17v358_ring_transfer_frame(&source, &destination), XR17V358_OK);
  EXPECT_EQ(xr17v358_ring_frame_count(&destination), 1U);

  xr17v358_ring_reset(&source, oversized_frame.size());
  ASSERT_EQ(xr17v358_ring_write_bytes(&source, oversized_frame.data(),
                                      oversized_frame.size()),
            oversized_frame.size());
  EXPECT_EQ(xr17v358_ring_transfer_frame(&source, &destination),
            XR17V358_ERROR_INVALID_FRAME);

  xr17v358_ring_reset(&source, 16U);
  xr17v358_ring_reset(&destination, std::numeric_limits<size_t>::max());
  destination.size = destination.capacity - 1U;
  ASSERT_EQ(xr17v358_ring_write_bytes(&source, frame.data(), frame.size()),
            frame.size());
  EXPECT_EQ(xr17v358_ring_transfer_frame(&source, &destination),
            XR17V358_ERROR_INVALID_FRAME);

  xr17v358_ring_reset(&multi_source, 16U);
  xr17v358_ring_reset(&multi_destination, XR17V358_MAX_ENCODED_BYTE_COUNT * 2U);
  ASSERT_EQ(xr17v358_ring_write_bytes(&multi_source, two_frames.data(),
                                      two_frames.size()),
            two_frames.size());
  EXPECT_EQ(xr17v358_ring_transfer_frames(&multi_source, &multi_destination, 1U,
                                          nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
  ASSERT_EQ(xr17v358_ring_transfer_frames(&multi_source, &multi_destination, 1U,
                                          &transferred),
            XR17V358_OK);
  EXPECT_EQ(transferred, 1U);
  EXPECT_EQ(xr17v358_ring_frame_count(&multi_source), 1U);
  EXPECT_EQ(xr17v358_ring_frame_count(&multi_destination), 1U);

  xr17v358_ring_reset(&multi_source, oversized_frame.size());
  xr17v358_ring_reset(&multi_destination, XR17V358_MAX_ENCODED_BYTE_COUNT * 2U);
  ASSERT_EQ(xr17v358_ring_write_bytes(&multi_source, oversized_frame.data(),
                                      oversized_frame.size()),
            oversized_frame.size());
  EXPECT_EQ(xr17v358_ring_transfer_frames(&multi_source, &multi_destination, 1U,
                                          &transferred),
            XR17V358_ERROR_INVALID_FRAME);
}

TEST(Xr17v358, InjectHelpersAndCapacityApisHandleValidation) {
  xr17v358_reset();

  size_t count = 0U;
  uint8_t byte = 0x11U;

  EXPECT_EQ(xr17v358_get_queue_capacity(), XR17V358_QUEUE_CAPACITY);
  EXPECT_EQ(xr17v358_inject_rx_frame_bytes(xr17v358_get_port_count(), &byte, 1U,
                                           &count),
            XR17V358_ERROR_INVALID_PORT);
  EXPECT_EQ(xr17v358_inject_tx_frame_bytes(0, &byte, 1U, nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_inject_queue_frame_bytes(0, nullptr, 1U, &count),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_inject_tx_frame_bytes(0, nullptr, 0U, &count),
            XR17V358_OK);
  EXPECT_EQ(count, 0U);
}

TEST(Xr17v358, ReceiveHandlesZeroLengthOversizeAndCapacityLimits) {
  xr17v358_reset();

  size_t received = 99U;
  uint8_t byte = 0x11U;
  std::vector<uint8_t> oversized_payload(xr17v358_get_queue_capacity() + 1U,
                                         0x22U);
  std::array<uint8_t, 3> existing_frame = {
      XR17V358_FRAME_DELIMITER,
      0x33U,
      XR17V358_FRAME_DELIMITER,
  };

  EXPECT_EQ(xr17v358_receive(0, &byte, 0U, &received), XR17V358_OK);
  EXPECT_EQ(received, 0U);

  EXPECT_EQ(xr17v358_receive(0, oversized_payload.data(),
                             oversized_payload.size(), &received),
            XR17V358_OK);
  EXPECT_EQ(received, 0U);

  xr17v358_ring_reset(&rx_fifo[0], 4U);
  ASSERT_EQ(xr17v358_ring_write_bytes(&rx_fifo[0], existing_frame.data(),
                                      existing_frame.size()),
            existing_frame.size());
  EXPECT_EQ(xr17v358_receive(0, &byte, 1U, &received), XR17V358_OK);
  EXPECT_EQ(received, 1U);
}

TEST(Xr17v358, TransferNextTxFrameCoversAllReachableOutcomes) {
  xr17v358_reset();

  constexpr std::array<uint8_t, 1> kPayload = {0xABU};
  size_t written = 0U;
  size_t read_count = 0U;
  std::array<uint8_t, 1> drained{};

  EXPECT_EQ(xr17v358_transfer_next_tx_frame(xr17v358_get_port_count()),
            XR17V358_ERROR_INVALID_PORT);
  EXPECT_EQ(xr17v358_transfer_next_tx_frame(0), XR17V358_ERROR_INVALID_FRAME);

  ASSERT_EQ(xr17v358_write(0, kPayload.data(), kPayload.size(), &written),
            XR17V358_OK);
  ASSERT_EQ(xr17v358_transfer_next_tx_frame(0), XR17V358_OK);
  ASSERT_EQ(xr17v358_queue_read(0, drained.data(), drained.size(), &read_count),
            XR17V358_OK);
  EXPECT_EQ(read_count, drained.size());
  EXPECT_EQ(drained[0], kPayload[0]);

  xr17v358_reset();
  for (size_t i = 0; i < XR17V358_FIFO_CAPACITY; ++i) {
    std::array<uint8_t, 3> frame = {
        XR17V358_FRAME_DELIMITER,
        0x11U,
        XR17V358_FRAME_DELIMITER,
    };
    ASSERT_EQ(
        xr17v358_ring_write_bytes(&tx_fifo[0], frame.data(), frame.size()),
        frame.size());
  }

  ASSERT_EQ(xr17v358_write(0, kPayload.data(), kPayload.size(), &written),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_transfer_next_tx_frame(0), XR17V358_ERROR_INVALID_FRAME);
}

TEST(Xr17v358, QueueReadReadAndPollPropagateOversizedFrameErrors) {
  xr17v358_reset();

  const std::vector<uint8_t> oversized_frame =
      make_raw_frame(XR17V358_MAX_ENCODED_BYTE_COUNT);
  const std::array<uint8_t, 4> bad_escape = {
      XR17V358_FRAME_DELIMITER,
      XR17V358_FRAME_ESCAPE,
      0x11U,
      XR17V358_FRAME_DELIMITER,
  };
  uint8_t byte = 0U;
  size_t count = 0U;

  ASSERT_EQ(xr17v358_inject_tx_frame_bytes(0, oversized_frame.data(),
                                           oversized_frame.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_queue_read(0, &byte, 1U, &count),
            XR17V358_ERROR_INVALID_FRAME);

  xr17v358_reset();
  ASSERT_EQ(xr17v358_inject_tx_frame_bytes(0, bad_escape.data(),
                                           bad_escape.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_queue_read(0, &byte, 1U, &count),
            XR17V358_ERROR_INVALID_FRAME);

  xr17v358_reset();
  ASSERT_EQ(xr17v358_inject_rx_frame_bytes(0, oversized_frame.data(),
                                           oversized_frame.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(xr17v358_read(0, &byte, 1U, &count), XR17V358_ERROR_INVALID_FRAME);

  xr17v358_reset();
  ASSERT_EQ(xr17v358_ring_write_bytes(&tx_queue[0], oversized_frame.data(),
                                      oversized_frame.size()),
            oversized_frame.size());
  EXPECT_EQ(xr17v358_poll_port(0), XR17V358_MESSAGE_NOT_READY);
  EXPECT_EQ(xr17v358_queue_read(0, &byte, 1U, &count), XR17V358_OK);
  EXPECT_EQ(count, 0U);
}

TEST(Xr17v358, WriteAndReadGuardAgainstShortWritesWithCorruptedState) {
  xr17v358_reset();

  constexpr std::array<uint8_t, 2> kPayload = {0x11U, 0x22U};
  const std::array<uint8_t, 4> rx_frame = {
      XR17V358_FRAME_DELIMITER,
      0x44U,
      0x55U,
      XR17V358_FRAME_DELIMITER,
  };
  std::vector<uint8_t> oversized_payload(xr17v358_get_queue_capacity() + 1U,
                                         0x77U);
  std::array<uint8_t, 2> output{};
  size_t count = 0U;

  EXPECT_EQ(xr17v358_write(0, kPayload.data(), 0U, &count), XR17V358_OK);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(xr17v358_write(0, oversized_payload.data(),
                           oversized_payload.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(count, 0U);

  tx_queue[0].capacity = std::numeric_limits<size_t>::max();
  tx_queue[0].size = tx_queue[0].capacity - 1U;
  tx_queue[0].tail = 0U;
  EXPECT_EQ(xr17v358_write(0, kPayload.data(), kPayload.size(), &count),
            XR17V358_ERROR_INVALID_FRAME);

  xr17v358_reset();
  ASSERT_EQ(xr17v358_inject_rx_frame_bytes(0, rx_frame.data(), rx_frame.size(),
                                           &count),
            XR17V358_OK);
  rx_queue[0].capacity = std::numeric_limits<size_t>::max();
  rx_queue[0].size = rx_queue[0].capacity - 1U;
  rx_queue[0].tail = 0U;
  EXPECT_EQ(xr17v358_read(0, output.data(), output.size(), &count),
            XR17V358_ERROR_INVALID_FRAME);
}

TEST(Xr17v358, DecodeAndTxFifoWriteCoverRemainingValidationCases) {
  xr17v358_reset();

  const std::array<uint8_t, 3> bad_delimiters = {0x00U, 0x11U, 0x7EU};
  const std::array<uint8_t, 3> trailing_escape = {
      XR17V358_FRAME_DELIMITER,
      XR17V358_FRAME_ESCAPE,
      XR17V358_FRAME_DELIMITER,
  };
  const std::array<uint8_t, 4> short_output_frame = {
      XR17V358_FRAME_DELIMITER,
      0x10U,
      0x21U,
      XR17V358_FRAME_DELIMITER,
  };
  const std::vector<uint8_t> too_long_payload_frame =
      make_raw_frame(XR17V358_QUEUE_CAPACITY + 1U);
  std::array<uint8_t, 1> small_output{};
  std::vector<uint8_t> large_output(XR17V358_QUEUE_CAPACITY + 1U);
  std::array<uint8_t, 0x2000> mmio{};
  uint8_t byte = 0x11U;
  size_t decoded_length = 0U;
  size_t written = 99U;

  EXPECT_EQ(xr17v358_decode_serial_data(
                0, bad_delimiters.data(), bad_delimiters.size(),
                small_output.data(), small_output.size(), &decoded_length),
            XR17V358_ERROR_INVALID_FRAME);
  EXPECT_EQ(xr17v358_decode_serial_data(
                0, trailing_escape.data(), trailing_escape.size(),
                small_output.data(), small_output.size(), &decoded_length),
            XR17V358_ERROR_INVALID_FRAME);
  EXPECT_EQ(xr17v358_decode_serial_data(
                0, too_long_payload_frame.data(), too_long_payload_frame.size(),
                large_output.data(), large_output.size(), &decoded_length),
            XR17V358_ERROR_INVALID_FRAME);
  EXPECT_EQ(xr17v358_decode_serial_data(
                0, short_output_frame.data(), short_output_frame.size(),
                small_output.data(), small_output.size(), &decoded_length),
            XR17V358_ERROR_INVALID_ARGUMENT);

  EXPECT_EQ(xr17v358_write_tx_fifo(mmio.data(), 0, nullptr, 0U, &written),
            XR17V358_OK);
  EXPECT_EQ(written, 0U);
  EXPECT_EQ(xr17v358_write_tx_fifo(mmio.data(), 0, nullptr, 1U, &written),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_write_tx_fifo(mmio.data(), 0, &byte, 1U, nullptr),
            XR17V358_ERROR_INVALID_ARGUMENT);
}

TEST(Xr17v358, PollPortRequiresCompleteFrameStillBufferedInRxFifo) {
  xr17v358_reset();

  constexpr std::array<uint8_t, 4> kRxData = {0x61U, 0x62U, 0x63U, 0x64U};
  std::array<uint8_t, 2> first_half{};
  size_t received = 0U;
  size_t read_count = 0U;

  ASSERT_EQ(xr17v358_receive(0, kRxData.data(), kRxData.size(), &received),
            XR17V358_OK);
  ASSERT_EQ(xr17v358_poll_port(0), XR17V358_MESSAGE_READY);
  ASSERT_EQ(xr17v358_read(0, first_half.data(), first_half.size(), &read_count),
            XR17V358_OK);
  ASSERT_EQ(read_count, first_half.size());
  EXPECT_EQ(xr17v358_fifo_level(0), 0U);
  EXPECT_EQ(xr17v358_poll_port(0), XR17V358_MESSAGE_NOT_READY);
}

TEST(Xr17v358, BranchCoverageExercisesAdditionalShortCircuitPaths) {
  xr17v358_reset();

  const xr17v358_port_config even_config = {
      38400U,
      XR17V358_STOP_BITS_1,
      XR17V358_PARITY_EVEN,
  };
  const xr17v358_port_config none_config = {
      19200U,
      XR17V358_STOP_BITS_1,
      XR17V358_PARITY_NONE,
  };
  const std::array<uint8_t, 2> short_frame = {
      XR17V358_FRAME_DELIMITER,
      0x11U,
  };
  const std::array<uint8_t, 3> bad_closing_frame = {
      XR17V358_FRAME_DELIMITER,
      0x22U,
      0x00U,
  };
  const std::array<uint8_t, 3> rx_frame = {
      XR17V358_FRAME_DELIMITER,
      0x33U,
      XR17V358_FRAME_DELIMITER,
  };
  const std::array<uint8_t, 3> tx_frame = {
      XR17V358_FRAME_DELIMITER,
      0x44U,
      XR17V358_FRAME_DELIMITER,
  };
  std::array<uint8_t, 1> output{};
  xr17v358_ring_buffer source{};
  xr17v358_ring_buffer destination{};
  size_t count = 0U;
  size_t decoded_length = 0U;
  uint8_t byte = 0x55U;

  EXPECT_EQ(xr17v358_initialize_port(0, &even_config), XR17V358_OK);
  EXPECT_EQ(xr17v358_initialize_port(1, &none_config), XR17V358_OK);
  EXPECT_EQ(xr17v358_decode_serial_data(0, nullptr, 3U, output.data(),
                                        output.size(), &decoded_length),
            XR17V358_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(xr17v358_decode_serial_data(0, short_frame.data(),
                                        short_frame.size(), output.data(),
                                        output.size(), &decoded_length),
            XR17V358_ERROR_INVALID_FRAME);
  EXPECT_EQ(xr17v358_decode_serial_data(0, bad_closing_frame.data(),
                                        bad_closing_frame.size(), output.data(),
                                        output.size(), &decoded_length),
            XR17V358_ERROR_INVALID_FRAME);

  EXPECT_EQ(xr17v358_read(0, nullptr, 0U, &count), XR17V358_OK);
  EXPECT_EQ(xr17v358_queue_read(0, nullptr, 0U, &count), XR17V358_OK);

  xr17v358_reset();
  EXPECT_EQ(xr17v358_queue_read(0, output.data(), output.size(), &count),
            XR17V358_OK);
  EXPECT_EQ(count, 0U);

  xr17v358_ring_reset(&tx_queue[0], 4U);
  tx_queue[0].size = 3U;
  EXPECT_EQ(xr17v358_write(0, &byte, 1U, &count), XR17V358_OK);
  EXPECT_EQ(count, 0U);

  xr17v358_reset();
  ASSERT_EQ(xr17v358_inject_rx_frame_bytes(0, rx_frame.data(), rx_frame.size(),
                                           &count),
            XR17V358_OK);
  xr17v358_ring_reset(&rx_queue[0], 1U);
  rx_queue[0].storage[0] = 0xAAU;
  rx_queue[0].size = 1U;
  std::array<uint8_t, 1> queued_output{};
  EXPECT_EQ(
      xr17v358_read(0, queued_output.data(), queued_output.size(), &count),
      XR17V358_OK);
  EXPECT_EQ(count, 1U);
  EXPECT_EQ(queued_output[0], 0xAAU);

  xr17v358_reset();
  for (size_t i = 0; i < XR17V358_FIFO_CAPACITY; ++i) {
    ASSERT_EQ(xr17v358_ring_write_bytes(&rx_fifo[0], rx_frame.data(),
                                        rx_frame.size()),
              rx_frame.size());
  }
  EXPECT_EQ(xr17v358_receive(0, &byte, 1U, &count), XR17V358_OK);
  EXPECT_EQ(count, 1U);

  xr17v358_reset();
  ASSERT_EQ(
      xr17v358_ring_write_bytes(&tx_fifo[0], tx_frame.data(), tx_frame.size()),
      tx_frame.size());
  EXPECT_EQ(xr17v358_queue_read(0, nullptr, 0U, &count), XR17V358_OK);
  EXPECT_EQ(count, 0U);

  xr17v358_ring_reset(&source, XR17V358_MAX_ENCODED_BYTE_COUNT);
  xr17v358_ring_reset(&destination, XR17V358_MAX_ENCODED_BYTE_COUNT * 2U);
  EXPECT_EQ(xr17v358_ring_transfer_frames(&source, &destination, 1U, &count),
            XR17V358_OK);
  EXPECT_EQ(count, 0U);
}
