// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_SURFACE_VK_HPP
#define SRC_GPU_VK_GPU_SURFACE_VK_HPP

#include <cmath>
#include <memory>
#include <skity/gpu/gpu_context_vk.hpp>

#include "src/gpu/gpu_surface_impl.hpp"

namespace skity {

class GPUTexture;

class GPUSurfaceVK : public GPUSurfaceImpl {
 public:
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
        sync_info_(sync_info) {}

  ~GPUSurfaceVK() override = default;

  GPUTextureFormat GetGPUFormat() const override { return format_; }

  std::shared_ptr<Pixmap> ReadPixels(const Rect& rect) override;

  void PrepareForSubmit(GPUCommandBuffer* command_buffer) override;

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;

  void OnFlush() override {}

 private:
  uint32_t target_width_ = 0;
  uint32_t target_height_ = 0;
  std::shared_ptr<GPUTexture> texture_ = {};
  GPUTextureFormat format_ = GPUTextureFormat::kInvalid;
  const GPUSurfaceSyncInfoVK* sync_info_ = nullptr;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_SURFACE_VK_HPP
