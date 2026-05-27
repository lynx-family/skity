// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include "common/golden_texture.hpp"

namespace skity {
namespace testing {

class GoldenTextureVK : public GoldenTexture {
 public:
  GoldenTextureVK(std::shared_ptr<Image> image, std::shared_ptr<Pixmap> pixmap)
      : GoldenTexture(std::move(image)), pixmap_(std::move(pixmap)) {}
  ~GoldenTextureVK() override = default;

  std::shared_ptr<skity::Pixmap> ReadPixels() override { return pixmap_; }

 private:
  std::shared_ptr<Pixmap> pixmap_;
};

}  // namespace testing
}  // namespace skity
