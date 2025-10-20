// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_STREAM_FILE_WRITE_STREAM_HPP
#define MODULE_IO_SRC_STREAM_FILE_WRITE_STREAM_HPP

#include <filesystem>
#include <fstream>
#include <skity/io/stream.hpp>

namespace skity {

class FileWriteStream : public WriteStream {
 public:
  FileWriteStream(std::filesystem::path path, std::ofstream file_stream);

  ~FileWriteStream() override;

  bool Write(void const* buffer, size_t size) override;

  bool Flush() override;

  size_t BytesWritten() const override { return byte_written_; }

 private:
  std::filesystem::path path_;
  std::ofstream file_stream_;
  size_t byte_written_ = 0;
};

}  // namespace skity

#endif  // MODULE_IO_SRC_STREAM_FILE_WRITE_STREAM_HPP
