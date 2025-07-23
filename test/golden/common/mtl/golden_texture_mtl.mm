// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/mtl/golden_texture_mtl.h"

namespace skity {
namespace testing {

GoldenTextureMTL::GoldenTextureMTL(std::shared_ptr<Image> image, id<MTLTexture> mtl_texture)
    : GoldenTexture(std::move(image)), mtl_texture_(mtl_texture) {}

}  // namespace testing
}  // namespace skity
