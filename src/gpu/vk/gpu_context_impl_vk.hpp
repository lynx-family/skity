// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_CONTEXT_IMPL_VK_HPP
#define SRC_GPU_VK_GPU_CONTEXT_IMPL_VK_HPP

#include <memory>
#include <skity/gpu/gpu_context.hpp>

namespace skity {

class VulkanContextState;

class GPUContextVK : public GPUContext {
 public:
  explicit GPUContextVK(std::unique_ptr<VulkanContextState> state);

  ~GPUContextVK() override;

  GPUBackendType GetBackendType() const override {
    return GPUBackendType::kVulkan;
  }

  std::unique_ptr<GPUSurface> CreateSurface(
      GPUSurfaceDescriptor* desc) override;

  std::shared_ptr<Texture> CreateTexture(TextureFormat format, uint32_t width,
                                         uint32_t height,
                                         AlphaType alpha_type) override;

  std::shared_ptr<Texture> CreateTextureWithDesc(
      const TextureDescriptor* desc) override;

  std::shared_ptr<Texture> WrapTexture(GPUBackendTextureInfo* info,
                                       ReleaseCallback callback,
                                       ReleaseUserData user_data) override;

  std::unique_ptr<GPURenderTarget> CreateRenderTarget(
      const GPURenderTargetDescriptor& desc) override;

  std::shared_ptr<Image> MakeSnapshot(
      std::unique_ptr<GPURenderTarget> render_target) override;

  void SetResourceCacheLimit(size_t size_in_bytes) override;

  const VulkanContextState* GetState() const { return state_.get(); }

 private:
  std::unique_ptr<VulkanContextState> state_;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_CONTEXT_IMPL_VK_HPP
