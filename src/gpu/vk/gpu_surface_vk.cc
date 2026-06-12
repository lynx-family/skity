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

const GPUSubmitInfo* GPUSurfaceVK::GetSubmitInfo() const {
  return has_submit_info_ ? &submit_info_ : nullptr;
}

HWRootLayer* GPUSurfaceVK::OnBeginNextFrame(bool clear) {
  auto root_layer = GetArenaAllocator()->Make<VKRootLayer>(
      target_width_, target_height_, texture_,
      Rect::MakeWH(GetWidth(), GetHeight()), GetGPUFormat());
  root_layer->SetClearSurface(clear);
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());
  return root_layer;
}

void GPUSurfaceVK::AddExternalWaitSemaphore(
    std::shared_ptr<GPUSemaphore> semaphore) {
  auto* vk_sem = static_cast<GPUSemaphoreVK*>(semaphore.get());
  submit_info_.AddWaitSemaphore(vk_sem->GetVkSemaphore(),
                                vk_sem->GetStageMask());
  external_semaphores_.push_back(std::move(semaphore));
}

void GPUSurfaceVK::OnFlush() {
  // canvas->Flush() has completed the submit. The VkSemaphore handles have
  // been consumed by the GPU. Remove the external semaphore entries from
  // submit_info_ and release the shared_ptr references.
  if (!external_semaphores_.empty()) {
    size_t count = external_semaphores_.size();
    submit_info_.wait_semaphores.resize(submit_info_.wait_semaphores.size() -
                                        count);
    submit_info_.wait_stage_masks.resize(submit_info_.wait_stage_masks.size() -
                                         count);
    external_semaphores_.clear();
  }
}

}  // namespace skity
