// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_SDF_GEN_HPP
#define SRC_RENDER_TEXT_SDF_GEN_HPP

#include <assert.h>

#include <memory>
#include <vector>

namespace skity {
namespace sdf {

template <typename T>
class Image {
 public:
  Image() : width_(0), height_(0), data_() {}

  Image(size_t width, size_t height) : width_(width), height_(height), data_() {
    data_.resize(width * height);
  }

  Image(size_t width, size_t height, const T& initial_value)
      : width_(width), height_(height), data_(width * height, initial_value) {}

  size_t GetWidth() const { return width_; }
  size_t GetHeight() const { return height_; }
  size_t GetSize() const { return data_.size(); }

  bool Set(size_t x, size_t y, const T& val) {
    const size_t index = GetIndex(x, y);
    data_[index] = val;
    return true;
  }

  const T& Get(size_t x, size_t y) const {
    const size_t index = GetIndex(x, y);
    return data_[index];
  }

  T* GetMutable(size_t x, size_t y) {
    const size_t index = GetIndex(x, y);
    return &data_[index];
  }

  T* GetRawData() { return data_.data(); }

 private:
  size_t GetIndex(size_t x, size_t y) const {
    assert(x < width_ && y < height_);
    return y * width_ + x;
  }

  size_t width_;
  size_t height_;
  std::vector<T> data_;
};

class SdfGen {
 public:
  static const Image<uint8_t> GenerateSdfImage(const Image<uint8_t>& src_image);
};

}  // namespace sdf
}  // namespace skity

#endif  // SRC_RENDER_TEXT_SDF_GEN_HPP
