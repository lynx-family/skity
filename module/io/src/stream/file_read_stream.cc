// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/stream/file_read_stream.hpp"

namespace skity {

FileReadStream::FileReadStream(std::filesystem::path path,
                               std::ifstream file_stream)
    : path_(std::move(path)), file_stream_(std::move(file_stream)) {}

FileReadStream::~FileReadStream() {
  if (file_stream_.is_open()) {
    file_stream_.close();
  }
}

size_t FileReadStream::Read(void* buffer, size_t size) {
  if (!file_stream_.is_open()) {
    return 0;
  }

  file_stream_.read(static_cast<char*>(buffer), size);

  return file_stream_.gcount();
}

size_t FileReadStream::Peek(void* buffer, size_t size) {
  if (!file_stream_.is_open()) {
    return 0;
  }

  auto current_pos = file_stream_.tellg();

  file_stream_.read(static_cast<char*>(buffer), size);

  auto peek_size = file_stream_.gcount();

  file_stream_.seekg(current_pos);

  return peek_size;
}

bool FileReadStream::IsAtEnd() const { return file_stream_.eof(); }

bool FileReadStream::Rewind() {
  if (!file_stream_.is_open()) {
    return false;
  }

  file_stream_.seekg(0, std::ios::beg);

  return true;
}

}  // namespace skity
