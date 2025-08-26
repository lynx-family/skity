// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/data_stream.hpp"

#include <cstring>

namespace skity {

bool DataStream::Seek(size_t offset) {
  if (offset > data_->Size()) {
    return false;
  }
  offset_ = offset;
  return true;
}

bool DataStream::Rewind() {
  offset_ = 0;
  return true;
}

size_t DataStream::Read(void* dst, size_t size) {
  if (offset_ + size > data_->Size()) {
    size = data_->Size() - offset_;
  }

  std::memcpy(dst, data_->Bytes() + offset_, size);

  offset_ += size;
  return size;
}

}  // namespace skity
