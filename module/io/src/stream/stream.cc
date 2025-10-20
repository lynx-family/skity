// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <array>
#include <cstring>
#include <skity/io/stream.hpp>

#include "src/stream/file_read_stream.hpp"
#include "src/stream/file_write_stream.hpp"

namespace skity {

constexpr size_t kMaxByteForU8 = 0xFD;
constexpr size_t kMaxByteForU16 = 0xFE;
constexpr size_t kMaxByteForU32 = 0xFF;

bool WriteStream::WriteU8(uint8_t value) { return Write(&value, 1); }

bool WriteStream::WriteU16(uint16_t value) { return Write(&value, 2); }

bool WriteStream::WriteU32(uint32_t value) { return Write(&value, 4); }

bool WriteStream::WritePackedUint(size_t value) {
  std::array<uint8_t, 5> data{};
  size_t len = 1;

  if (value <= kMaxByteForU8) {
    data[0] = value;
    len = 1;
  } else if (value <= 0xFFFF) {
    uint16_t value16 = value;
    data[0] = kMaxByteForU16;

    std::memcpy(data.data() + 1, &value16, 2);
    len = 3;
  } else {
    uint32_t value32 = value;
    data[0] = kMaxByteForU32;
    std::memcpy(data.data() + 1, &value32, 4);
    len = 5;
  }

  return Write(data.data(), len);
}

bool WriteStream::WriteText(const std::string& text) {
  return Write(text.data(), text.size());
}

bool WriteStream::WriteText(const char* text) {
  return Write(text, std::strlen(text));
}

bool WriteStream::WriteNewLine() { return Write("\n", std::strlen("\n")); }

bool WriteStream::WriteBool(bool value) { return WriteU8(value ? 1 : 0); }

bool WriteStream::WriteFloat(float value) {
  return Write(&value, sizeof(float));
}

size_t WriteStream::PackedUintSize(size_t value) {
  if (value <= kMaxByteForU8) {
    return 1;
  } else if (value <= 0xFFFF) {
    return 3;
  } else {
    return 5;
  }
}

std::unique_ptr<WriteStream> WriteStream::CreateFileStream(
    const std::string& path) {
  std::filesystem::path fs_path(path);

  if (std::filesystem::exists(fs_path)) {
    std::filesystem::remove(fs_path);
  }

  std::ofstream file_stream(path, std::ios::binary | std::ios::out);

  if (!file_stream.is_open()) {
    return {};
  }

  return std::make_unique<FileWriteStream>(fs_path, std::move(file_stream));
}

bool ReadStream::ReadI8(int8_t* value) {
  return this->Read(value, sizeof(int8_t)) == sizeof(int8_t);
}

bool ReadStream::ReadI16(int16_t* value) {
  return this->Read(value, sizeof(int16_t)) == sizeof(int16_t);
}

bool ReadStream::ReadI32(int32_t* value) {
  return this->Read(value, sizeof(int32_t)) == sizeof(int32_t);
}

bool ReadStream::ReadFloat(float* value) {
  return this->Read(value, sizeof(float)) == sizeof(float);
}

bool ReadStream::ReadPackedUint(size_t* value) {
  uint8_t byte;

  if (!Read(&byte, sizeof(byte))) {
    return false;
  }

  if (kMaxByteForU16 == byte) {
    uint16_t value16;

    if (!ReadU16(&value16)) {
      return false;
    }

    *value = value16;
  } else if (kMaxByteForU32 == byte) {
    uint32_t value32;

    if (!ReadU32(&value32)) {
      return false;
    }

    *value = value32;
  } else {
    *value = byte;
  }

  return true;
}

std::unique_ptr<ReadStream> ReadStream::CreateFromFile(
    const std::string& path) {
  std::filesystem::path fs_path(path);

  if (!std::filesystem::exists(fs_path)) {
    return {};
  }

  std::ifstream file_stream(path, std::ios::binary | std::ios::in);

  if (!file_stream.is_open()) {
    return {};
  }

  return std::make_unique<FileReadStream>(std::move(fs_path),
                                          std::move(file_stream));
}

}  // namespace skity
