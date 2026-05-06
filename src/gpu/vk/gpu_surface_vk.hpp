// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_SURFACE_VK_HPP
#define SRC_GPU_VK_GPU_SURFACE_VK_HPP

#include <cmath>
#include <memory>
#include <skity/gpu/gpu_context_vk.hpp>

#include "src/gpu/gpu_surface_impl.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"

namespace skity {

class GPUTexture;

class GPUSurfaceVK : public GPUSurfaceImpl {
 public:
  struct PresentInfo {
    const void* owner = nullptr;
    uint32_t image_index = 0;
  };

  GPUSurfaceVK(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
               std::shared_ptr<GPUTexture> texture, GPUTextureFormat format,
               const GPUSurfaceSyncInfoVK* sync_info)
      : GPUSurfaceImpl(desc, ctx),
        target_width_(static_cast<uint32_t>(
            std::floor(static_cast<float>(desc.width) * desc.content_scale))),
        target_height_(static_cast<uint32_t>(
            std::floor(static_cast<float>(desc.height) * desc.content_scale))),
        texture_(std::move(texture)),
        format_(format),
        has_submit_info_(sync_info != nullptr) {
    if (sync_info != nullptr) {
      submit_info_.wait_semaphore = sync_info->wait_semaphore;
      submit_info_.wait_dst_stage_mask = sync_info->wait_dst_stage_mask;
      submit_info_.signal_semaphore = sync_info->signal_semaphore;
      submit_info_.signal_fence = sync_info->signal_fence;
    }
  }

  ~GPUSurfaceVK() override = default;

  GPUTextureFormat GetGPUFormat() const override { return format_; }

  std::shared_ptr<Pixmap> ReadPixels(const Rect& rect) override;

  const GPUSubmitInfo* GetSubmitInfo() const override;

  void SetPresentInfo(const PresentInfo& present_info) {
    has_present_info_ = true;
    present_info_ = present_info;
  }

  const PresentInfo* GetPresentInfo() const {
    return has_present_info_ ? &present_info_ : nullptr;
  }

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;

  void OnFlush() override {}

 private:
  uint32_t target_width_ = 0;
  uint32_t target_height_ = 0;
  std::shared_ptr<GPUTexture> texture_ = {};
  GPUTextureFormat format_ = GPUTextureFormat::kInvalid;
  bool has_submit_info_ = false;
  GPUSubmitInfoVK submit_info_ = {};
  bool has_present_info_ = false;
  PresentInfo present_info_ = {};
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_SURFACE_VK_HPP
