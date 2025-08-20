// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_SURFACE_VK_HPP
#define SRC_GPU_VK_GPU_SURFACE_VK_HPP

#include <volk.h>

#include <memory>
#include <skity/gpu/gpu_context_vk.hpp>
#include <skity/gpu/gpu_surface.hpp>

#include "src/gpu/gpu_surface_impl.hpp"

namespace skity {

class GPUDeviceVk;
class GPUTextureVk;
class Canvas;
class Pixmap;
class HWRootLayer;

// Using the public GPUSurfaceDescriptorVk from gpu_context_vk.hpp

class GPUSurfaceVk : public GPUSurfaceImpl {
 public:
  GPUSurfaceVk(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx);
  ~GPUSurfaceVk() override;

  bool Initialize(const GPUSurfaceDescriptorVk& vk_desc);
  void SetTargetTexture(std::shared_ptr<GPUTexture> texture);

  std::shared_ptr<Pixmap> ReadPixels(const Rect& rect) override;

  GPUTextureFormat GetGPUFormat() const override {
    return GPUTextureFormat::kRGBA8Unorm;
  }

  static std::unique_ptr<GPUSurfaceVk> Create(
      GPUContextImpl* ctx, const GPUSurfaceDescriptorVk& desc);

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;
  void OnFlush() override;

  std::shared_ptr<GPUTexture> GetTargetTexture() const {
    return target_texture_;
  }

 private:
  bool CreateRenderTarget();
  void CreateFramebuffer();
  void DestroyFramebuffer();

  std::shared_ptr<GPUTexture> target_texture_;
  VkFormat format_ = VK_FORMAT_R8G8B8A8_UNORM;

  VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_SURFACE_VK_HPP
