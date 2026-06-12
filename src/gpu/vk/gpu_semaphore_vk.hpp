// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_SEMAPHORE_VK_HPP
#define SRC_GPU_VK_GPU_SEMAPHORE_VK_HPP

#include <vulkan/vulkan.h>

#include <memory>
#include <skity/gpu/gpu_semaphore.hpp>

namespace skity {

class VulkanContextState;

/**
 * Vulkan implementation of GPUSemaphore.
 *
 * Owns a VkSemaphore created via vkCreateSemaphore. Destroyed via
 * vkDestroySemaphore when the last shared_ptr reference is released.
 *
 * Lifecycle:
 * - Created by GPUContextVK::CreateSemaphore()
 * - Each frame: re-imported with a new fd via GPUContextVK::ImportSemaphore()
 * - Passed to GPUSurfaceVK::AddExternalWaitSemaphore() before flush
 * - Destroyed when the app releases its shared_ptr
 */
class GPUSemaphoreVK : public GPUSemaphore {
 public:
  ~GPUSemaphoreVK() override;

  VkSemaphore GetVkSemaphore() const { return semaphore_; }
  VkPipelineStageFlags GetStageMask() const { return stage_mask_; }
  void SetStageMask(VkPipelineStageFlags mask) { stage_mask_ = mask; }

 private:
  friend class GPUContextVK;
  GPUSemaphoreVK(std::shared_ptr<const VulkanContextState> state,
                 VkSemaphore semaphore);

  std::shared_ptr<const VulkanContextState> state_;
  VkSemaphore semaphore_ = VK_NULL_HANDLE;
  VkPipelineStageFlags stage_mask_ = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_SEMAPHORE_VK_HPP
