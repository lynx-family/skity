// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_COMMAND_BUFFER_VK_HPP
#define SRC_GPU_VK_GPU_COMMAND_BUFFER_VK_HPP

#include <volk.h>

#include <vector>

#include "src/gpu/gpu_command_buffer.hpp"
#include "src/gpu/vk/sync_objects_vk.hpp"

namespace skity {

class GPUDeviceVk;
class GPURenderPassVk;
class GPUBlitPassVk;

class GPUCommandBufferVk : public GPUCommandBuffer {
 public:
  explicit GPUCommandBufferVk(GPUDeviceVk* device);
  ~GPUCommandBufferVk() override;

  bool Initialize();
  void Reset();

  std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor& desc) override;

  std::shared_ptr<GPUBlitPass> BeginBlitPass() override;

  bool Submit() override;

  VkCommandBuffer GetVkCommandBuffer() const { return command_buffer_; }
  GPUDeviceVk* GetDevice() const { return device_; }

  // Synchronization support
  void AddMemoryBarrier(const VkMemoryBarrier& barrier);
  void AddImageBarrier(const VkImageBarrier& barrier);
  void AddBufferBarrier(const VkBufferBarrier& barrier);
  void ExecuteBarriers();
  void TransitionImageLayout(
      VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
      VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT);

 private:
  bool BeginRecording();
  bool EndRecording();

  GPUDeviceVk* device_ = nullptr;
  VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
  VkCommandPool command_pool_ = VK_NULL_HANDLE;

  bool is_recording_ = false;
  std::vector<std::shared_ptr<GPURenderPassVk>> render_passes_;
  std::vector<std::shared_ptr<GPUBlitPassVk>> blit_passes_;

  // Synchronization manager
  std::unique_ptr<VkSyncManager> sync_manager_;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_COMMAND_BUFFER_VK_HPP
