// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_surface_vk.hpp"

#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/render/hw/vk/vk_root_layer.hpp"

namespace skity {

std::shared_ptr<Pixmap> GPUSurfaceVK::ReadPixels(const Rect& rect) {
  (void)rect;
  return {};
}

void GPUSurfaceVK::PrepareForSubmit(GPUCommandBuffer* command_buffer) {
  if (command_buffer == nullptr) {
    return;
  }

  auto* command_buffer_vk = static_cast<GPUCommandBufferVK*>(command_buffer);
  command_buffer_vk->SetSubmitSyncInfo(sync_info_);
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
