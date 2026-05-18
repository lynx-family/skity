// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_surface_vk.hpp"

namespace skity {

std::shared_ptr<Pixmap> GPUSurfaceVK::ReadPixels(const Rect& rect) {
  (void)rect;
  return {};
}

const GPUSubmitInfo* GPUSurfaceVK::GetSubmitInfo() const {
  return has_submit_info_ ? &submit_info_ : nullptr;
}

HWRootLayer* GPUSurfaceVK::OnBeginNextFrame(bool clear) {
  // VKRootLayer integration will be added with the HW render layer PR
  return nullptr;
}

}  // namespace skity
