// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <Metal/Metal.h>

#include <skity/gpu/texture.hpp>

#include "common/golden_texture.hpp"

namespace skity {
namespace testing {

class GoldenTextureMTL : public GoldenTexture {
 public:
  GoldenTextureMTL(std::shared_ptr<Image> image, id<MTLTexture> mtl_texture);

  ~GoldenTextureMTL() override = default;

  id<MTLTexture> GetMTLTexture() const { return mtl_texture_; }

 private:
  id<MTLTexture> mtl_texture_;
};

}  // namespace testing
}  // namespace skity
