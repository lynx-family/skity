// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_COMMAND_BUFFER_VK_HPP
#define SRC_GPU_VK_GPU_COMMAND_BUFFER_VK_HPP

#include <vulkan/vulkan.h>

#include <functional>
#include <memory>
#include <skity/gpu/gpu_context_vk.hpp>
#include <vector>

#include "src/gpu/gpu_command_buffer.hpp"

namespace skity {

class GPUBufferVK;
class VulkanContextState;

class GPUSubmitInfoVK : public GPUSubmitInfo {
 public:
  GPUBackendType GetBackendType() const override {
    return GPUBackendType::kVulkan;
  }

  VkSemaphore wait_semaphore = VK_NULL_HANDLE;
  VkPipelineStageFlags wait_dst_stage_mask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSemaphore signal_semaphore = VK_NULL_HANDLE;
  VkFence signal_fence = VK_NULL_HANDLE;
};

class GPUCommandBufferVK : public GPUCommandBuffer {
 public:
  explicit GPUCommandBufferVK(std::shared_ptr<const VulkanContextState> state);

  ~GPUCommandBufferVK() override;

  bool Init();

  std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor& desc) override;

  std::shared_ptr<GPUBlitPass> BeginBlitPass() override;

  void HoldResource(std::shared_ptr<void> resource) override;

  bool Submit(const GPUSubmitInfo* submit_info = nullptr) override;

  VkCommandBuffer GetCommandBuffer() const { return command_buffer_; }

  void ApplyDebugLabelsIfNeeded();

  void RecordStageBuffer(std::unique_ptr<GPUBufferVK> buffer);

  void RecordCleanupAction(std::function<void()> action);

 private:
  void Reset();

  std::shared_ptr<const VulkanContextState> state_ = {};
  VkCommandPool command_pool_ = VK_NULL_HANDLE;
  VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
  bool recording_ = false;
  bool submitted_ = false;
  std::vector<std::unique_ptr<GPUBufferVK>> stage_buffers_ = {};
  std::vector<std::function<void()>> cleanup_actions_ = {};
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_COMMAND_BUFFER_VK_HPP
