// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_CONTEXT_IMPL_VK_HPP
#define SRC_GPU_VK_GPU_CONTEXT_IMPL_VK_HPP

#include <memory>

#include "src/gpu/gpu_context_impl.hpp"

namespace skity {

class VulkanContextState;

class GPUContextVK : public GPUContextImpl {
 public:
  explicit GPUContextVK(std::shared_ptr<VulkanContextState> state);

  ~GPUContextVK() override;

  GPUBackendType GetBackendType() const override {
    return GPUBackendType::kVulkan;
  }

  std::unique_ptr<GPUSurface> CreateSurface(
      GPUSurfaceDescriptor* desc) override;

  std::unique_ptr<GPUPresenter> CreatePresenter(
      GPUPresenterDescriptor* desc) override;

  const VulkanContextState* GetState() const { return state_.get(); }

 protected:
  std::unique_ptr<GPUDevice> CreateGPUDevice() override;

  std::shared_ptr<GPUTexture> OnWrapTexture(GPUBackendTextureInfo* info,
                                            ReleaseCallback callback,
                                            ReleaseUserData user_data) override;

  std::unique_ptr<GPURenderTarget> OnCreateRenderTarget(
      const GPURenderTargetDescriptor& desc,
      std::shared_ptr<Texture> texture) override;

  std::shared_ptr<Data> OnReadPixels(
      const std::shared_ptr<GPUTexture>& texture) const override;

 private:
  std::shared_ptr<VulkanContextState> state_;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_CONTEXT_IMPL_VK_HPP
