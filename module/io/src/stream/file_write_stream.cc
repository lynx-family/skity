// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/stream/file_write_stream.hpp"

namespace skity {

FileWriteStream::FileWriteStream(std::filesystem::path path,
                                 std::ofstream file_stream)
    : path_(path), file_stream_(std::move(file_stream)) {}

FileWriteStream::~FileWriteStream() {
  Flush();

  if (file_stream_.is_open()) {
    file_stream_.close();
  }
}

bool FileWriteStream::Write(void const* buffer, size_t size) {
  if (!file_stream_.is_open()) {
    return false;
  }

  file_stream_.write(reinterpret_cast<const char*>(buffer), size);

  byte_written_ += size;

  return true;
}

bool FileWriteStream::Flush() {
  if (!file_stream_.is_open()) {
    return false;
  }

  file_stream_.flush();

  return true;
}

}  // namespace skity
