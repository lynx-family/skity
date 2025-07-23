// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <skity/skity.hpp>

namespace skity {
namespace testing {

class GoldenTexture {
 public:
  GoldenTexture(std::shared_ptr<Image> image) : image_(std::move(image)) {}

  virtual ~GoldenTexture() = default;

  const std::shared_ptr<Image>& GetImage() const { return image_; }

  std::shared_ptr<skity::Pixmap> ReadPixels();

 private:
  std::shared_ptr<Image> image_;
};

}  // namespace testing
}  // namespace skity
