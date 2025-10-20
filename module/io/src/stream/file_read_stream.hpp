// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_STREAM_FILE_READ_STREAM_HPP
#define MODULE_IO_SRC_STREAM_FILE_READ_STREAM_HPP

#include <filesystem>
#include <fstream>
#include <skity/io/stream.hpp>

namespace skity {

class FileReadStream : public ReadStream {
 public:
  FileReadStream(std::filesystem::path path, std::ifstream file_stream);

  ~FileReadStream() override;

  size_t Read(void* buffer, size_t size) override;
  size_t Peek(void* buffer, size_t size) override;

  bool IsAtEnd() const override;

  bool Rewind() override;

 private:
  std::filesystem::path path_;
  std::ifstream file_stream_;
};

}  // namespace skity

#endif  // MODULE_IO_SRC_STREAM_FILE_READ_STREAM_HPP
