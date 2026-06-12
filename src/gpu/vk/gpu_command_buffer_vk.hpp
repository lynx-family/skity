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

  /**
   * Add a wait semaphore to the submit info.
   * For swapchain acquire, this is called at surface creation time.
   * For external texture sync (e.g. GL fence), this is called per-frame
   * before canvas->Flush().
   */
  void AddWaitSemaphore(VkSemaphore semaphore,
                        VkPipelineStageFlags stage_mask) {
    wait_semaphores.push_back(semaphore);
    wait_stage_masks.push_back(stage_mask);
  }

  // Wait semaphores: acquire semaphore + any external semaphores
  std::vector<VkSemaphore> wait_semaphores = {};
  std::vector<VkPipelineStageFlags> wait_stage_masks = {};

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
  std::vector<std::unique_ptr<GPUBufferVK>> stage_buffers_;
  std::vector<std::function<void()>> cleanup_actions_;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_COMMAND_BUFFER_VK_HPP
