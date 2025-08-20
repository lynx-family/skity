// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_CONTEXT_IMPL_VK_HPP
#define SRC_GPU_VK_GPU_CONTEXT_IMPL_VK_HPP

#include <skity/gpu/gpu_context_vk.hpp>

#include "src/gpu/gpu_context_impl.hpp"

namespace skity {

class GPUContextImplVk : public GPUContextImpl {
 public:
  GPUContextImplVk();
  ~GPUContextImplVk() override = default;

  /**
   * Initialize the context with custom device preferences
   */
  bool InitWithPreferences(const VkDevicePreferences& preferences);

  /**
   * Initialize the context using existing Vulkan objects
   */
  bool InitWithExistingObjects(uint64_t instance, uint64_t device,
                               uint64_t queue, uint32_t queue_family_index);

  std::unique_ptr<GPUSurface> CreateSurface(
      GPUSurfaceDescriptor* desc) override;

  std::unique_ptr<GPUSurface> CreateFxaaSurface(
      GPUSurfaceDescriptor* desc) override;

 protected:
  std::unique_ptr<GPUDevice> CreateGPUDevice() override;

  std::shared_ptr<GPUTexture> OnWrapTexture(GPUBackendTextureInfo* info,
                                            ReleaseCallback callback,
                                            ReleaseUserData user_data) override;

  std::shared_ptr<Data> OnReadPixels(
      const std::shared_ptr<GPUTexture>& texture) const override;

  std::unique_ptr<GPURenderTarget> OnCreateRenderTarget(
      const GPURenderTargetDescriptor& desc,
      std::shared_ptr<Texture> texture) override;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_CONTEXT_IMPL_VK_HPP