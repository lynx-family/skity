// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_surface_vk.hpp"

#include "src/render/hw/vk/vk_root_layer.hpp"

namespace skity {

std::shared_ptr<Pixmap> GPUSurfaceVK::ReadPixels(const Rect& rect) {
  (void)rect;
  return {};
}

HWRootLayer* GPUSurfaceVK::OnBeginNextFrame(bool clear) {
  auto root_layer = GetArenaAllocator()->Make<VKRootLayer>(
      texture_, Rect::MakeWH(GetWidth(), GetHeight()), GetGPUFormat());
  root_layer->SetClearSurface(clear);
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());
  return root_layer;
}

}  // namespace skity
