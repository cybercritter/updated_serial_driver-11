/**
 * @file helpers.h
 * @brief Small C++ helpers used by the local test suite.
 */
#ifndef FIFOUPDATES_TESTS_HELPERS_H
#define FIFOUPDATES_TESTS_HELPERS_H

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace test_helpers {

/**
 * @brief Deleter used with `std::unique_ptr` for `FILE*` handles.
 */
struct FileCloser {
  /**
   * @brief Close the owned file handle when present.
   * @param file File handle to close.
   */
  void operator()(FILE *file) const {
    if (file != nullptr) {
      fclose(file);
    }
  }
};

/** Smart-pointer wrapper for temporary file handles used in tests. */
using ScopedFile = std::unique_ptr<FILE, FileCloser>;

/**
 * @brief Create a temporary file wrapped in a scoped handle.
 * @return Scoped temporary file handle.
 */
inline ScopedFile make_temp_file() { return ScopedFile(tmpfile()); }

/**
 * @brief Read a file into a caller-provided fixed-size character buffer.
 * @tparam BufferSize Compile-time size of the character buffer.
 * @param file File handle to read from.
 * @param buffer Character buffer used to store the file contents.
 * @return String containing the bytes read from @p file.
 */
template <size_t BufferSize>
inline std::string read_file(FILE *file, std::array<char, BufferSize> *buffer) {
  const size_t bytes = fread(buffer->data(), 1, buffer->size() - 1U, file);
  (*buffer)[bytes] = '\0';
  return std::string(buffer->data());
}

/**
 * @brief Read an entire file into a temporary internal buffer.
 * @param file File handle to read from.
 * @return String containing the bytes read from @p file.
 */
inline std::string read_file(FILE *file) {
  std::array<char, 2048> buffer{};

  rewind(file);
  return read_file(file, &buffer);
}

/**
 * @brief Build a byte vector containing incrementing 8-bit values.
 * @param count Number of bytes to generate.
 * @return Vector of bytes with values `0, 1, 2, ...`.
 */
inline std::vector<uint8_t> make_incrementing_bytes(size_t count) {
  std::vector<uint8_t> bytes(count);

  for (size_t i = 0; i < bytes.size(); ++i) {
    bytes[i] = static_cast<uint8_t>(i & 0xFFU);
  }

  return bytes;
}

}  // namespace test_helpers

#endif
