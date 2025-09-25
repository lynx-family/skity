// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_DEVICE_VK_HPP
#define SRC_GPU_VK_GPU_DEVICE_VK_HPP

#include <vk_mem_alloc.h>
#include <volk.h>

#include <memory>
#include <vector>

#include "src/gpu/gpu_device.hpp"

namespace skity {

class GPUPipelineCacheVk;

struct QueueFamilyIndices {
  uint32_t graphics_family = UINT32_MAX;
  uint32_t present_family = UINT32_MAX;
  uint32_t compute_family = UINT32_MAX;
  uint32_t transfer_family = UINT32_MAX;

  bool IsComplete() const {
    return graphics_family != UINT32_MAX && present_family != UINT32_MAX;
  }
};

class GPUDeviceVk : public GPUDevice {
 public:
  GPUDeviceVk();
  ~GPUDeviceVk() override;

  bool Init();

  std::unique_ptr<GPUBuffer> CreateBuffer(GPUBufferUsageMask usage) override;

  std::shared_ptr<GPUShaderFunction> CreateShaderFunction(
      const GPUShaderFunctionDescriptor& desc) override;

  std::unique_ptr<GPURenderPipeline> CreateRenderPipeline(
      const GPURenderPipelineDescriptor& desc) override;

  std::unique_ptr<GPURenderPipeline> ClonePipeline(
      GPURenderPipeline* base,
      const GPURenderPipelineDescriptor& desc) override;

  std::shared_ptr<GPUCommandBuffer> CreateCommandBuffer() override;

  std::shared_ptr<GPUSampler> CreateSampler(
      const GPUSamplerDescriptor& desc) override;

  std::shared_ptr<GPUTexture> CreateTexture(
      const GPUTextureDescriptor& desc) override;

  bool CanUseMSAA() override;

  uint32_t GetBufferAlignment() override;

  uint32_t GetMaxTextureSize() override;

  // Vulkan-specific getters
  VkDevice GetDevice() const { return device_; }
  VkPhysicalDevice GetPhysicalDevice() const { return physical_device_; }
  VkQueue GetGraphicsQueue() const { return graphics_queue_; }
  VkQueue GetPresentQueue() const { return present_queue_; }
  VkCommandPool GetCommandPool() const { return command_pool_; }
  VmaAllocator GetAllocator() const { return allocator_; }
  const QueueFamilyIndices& GetQueueFamilyIndices() const {
    return queue_family_indices_;
  }

  // Command buffer utilities
  VkCommandBuffer BeginSingleTimeCommands();
  void EndSingleTimeCommands(VkCommandBuffer command_buffer);

  // Pipeline cache access
  GPUPipelineCacheVk* GetPipelineCache() const { return pipeline_cache_.get(); }

  // Extension support queries
  bool HasSynchronization2Support() const {
    return synchronization2_supported_;
  }

  // Render pass creation for pipeline compatibility
  VkRenderPass GetCompatibleRenderPass(
      VkFormat format = VK_FORMAT_B8G8R8A8_SRGB,
      bool needsDepthStencil = false);

 private:
  bool CreateLogicalDevice();
  bool CreateCommandPool();
  bool CreateVmaAllocator();
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
  bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
  void CheckExtensionSupport();

 private:
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphics_queue_ = VK_NULL_HANDLE;
  VkQueue present_queue_ = VK_NULL_HANDLE;
  VkCommandPool command_pool_ = VK_NULL_HANDLE;
  VmaAllocator allocator_ = VK_NULL_HANDLE;
  std::unique_ptr<GPUPipelineCacheVk> pipeline_cache_;

  QueueFamilyIndices queue_family_indices_;
  VkPhysicalDeviceProperties device_properties_;
  VkPhysicalDeviceFeatures device_features_;

  // Extension support flags
  bool synchronization2_supported_ = false;

  // Cache for compatible render passes
  VkRenderPass default_render_pass_ = VK_NULL_HANDLE;
  VkRenderPass depth_stencil_render_pass_ = VK_NULL_HANDLE;
  bool CreateDefaultRenderPass();
  bool CreateDepthStencilRenderPass();
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_DEVICE_VK_HPP