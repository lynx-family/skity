// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_DATA_STREAM_HPP
#define MODULE_CODEC_SRC_CODEC_DATA_STREAM_HPP

#include <skity/io/data.hpp>

namespace skity {

class DataStream {
 public:
  DataStream(std::shared_ptr<Data> data) : data_(std::move(data)), offset_(0) {}

  ~DataStream() = default;

  bool Seek(size_t offset);

  bool Rewind();

  size_t Read(void* dst, size_t size);

  const std::shared_ptr<Data>& GetData() const { return data_; }

 private:
  std::shared_ptr<Data> data_;
  size_t offset_;
};

}  // namespace skity

#endif  // MODULE_CODEC_SRC_CODEC_DATA_STREAM_HPP
