// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/layer/hw_root_layer.hpp"

namespace skity {

HWRootLayer::HWRootLayer(uint32_t width, uint32_t height, const Rect &bounds,
                         GPUTextureFormat format)
    : HWLayer(Matrix(glm::mat4(1.f)), 1, bounds, width, height) {
  SetColorFormat(format);
}

}  // namespace skity
