// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_INCLUDE_SKITY_IO_STREAM_HPP
#define MODULE_IO_INCLUDE_SKITY_IO_STREAM_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <skity/macros.hpp>
#include <string>

namespace skity {

class SKITY_API WriteStream {
 public:
  WriteStream() = default;

  virtual ~WriteStream() = default;

  WriteStream(const WriteStream&) = delete;
  WriteStream& operator=(const WriteStream&) = delete;

  virtual bool Write(void const* buffer, size_t size) = 0;

  virtual bool Flush() = 0;

  virtual size_t BytesWritten() const = 0;

  bool WriteU8(uint8_t value);
  bool WriteU16(uint16_t value);
  bool WriteU32(uint32_t value);
  bool WritePackedUint(size_t value);
  bool WriteText(const std::string& text);
  bool WriteText(const char* text);
  bool WriteNewLine();

  bool WriteBool(bool value);
  bool WriteFloat(float value);

  static size_t PackedUintSize(size_t value);

  static std::unique_ptr<WriteStream> CreateFileStream(const std::string& path);
};

class SKITY_API ReadStream {
 public:
  ReadStream() = default;
  virtual ~ReadStream() = default;

  /**
   * Read size of `size` from stream to `buffer`.
   * If buffer is nullptr, will skip `size` bytes.
   *
   * @return size_t return size of bytes actually read.
   */
  virtual size_t Read(void* buffer, size_t size) = 0;

  /**
   * Skip size of `size` bytes in stream.
   *
   * @return size_t return size of bytes actually skipped.
   */
  size_t Skip(size_t size) { return Read(nullptr, size); }

  /**
   * Peek size of `size` bytes in stream.
   *
   * @return size_t return size of bytes actually peeked.
   */
  virtual size_t Peek(void* buffer, size_t size) = 0;

  virtual bool IsAtEnd() const = 0;

  bool ReadI8(int8_t* value);
  bool ReadI16(int16_t* value);
  bool ReadI32(int32_t* value);

  bool ReadU8(uint8_t* value) {
    return ReadI8(reinterpret_cast<int8_t*>(value));
  }
  bool ReadU16(uint16_t* value) {
    return ReadI16(reinterpret_cast<int16_t*>(value));
  }
  bool ReadU32(uint32_t* value) {
    return ReadI32(reinterpret_cast<int32_t*>(value));
  }

  bool ReadBool(bool* value) {
    uint8_t i;
    if (!ReadU8(&i)) {
      return false;
    }

    *value = (i != 0);

    return true;
  }

  bool ReadFloat(float* value);
  bool ReadPackedUint(size_t* value);

  /**
   * Rewind stream to beginning.
   *
   * @return true if rewind success.
   */
  virtual bool Rewind() { return false; }

  static std::unique_ptr<ReadStream> CreateFromFile(const std::string& path);
};

}  // namespace skity

#endif  // MODULE_IO_INCLUDE_SKITY_IO_STREAM_HPP
