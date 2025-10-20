// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_command_buffer_vk.hpp"

#include "src/gpu/vk/gpu_blit_pass_vk.hpp"
#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/gpu_render_pass_vk.hpp"
#include "src/logging.hpp"

namespace skity {

GPUCommandBufferVk::GPUCommandBufferVk(GPUDeviceVk* device)
    : device_(device), sync_manager_(std::make_unique<VkSyncManager>(device)) {}

GPUCommandBufferVk::~GPUCommandBufferVk() {
  if (command_buffer_ != VK_NULL_HANDLE && device_) {
    vkFreeCommandBuffers(device_->GetDevice(), device_->GetCommandPool(), 1,
                         &command_buffer_);
  }
}

bool GPUCommandBufferVk::Initialize() {
  if (!device_) {
    LOGE("Invalid device for command buffer initialization");
    return false;
  }

  command_pool_ = device_->GetCommandPool();

  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = command_pool_;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = 1;

  VkResult result = vkAllocateCommandBuffers(device_->GetDevice(), &alloc_info,
                                             &command_buffer_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to allocate command buffer: %d", result);
    return false;
  }

  return true;
}

void GPUCommandBufferVk::Reset() {
  if (command_buffer_ != VK_NULL_HANDLE) {
    vkResetCommandBuffer(command_buffer_, 0);
  }

  render_passes_.clear();
  blit_passes_.clear();
  is_recording_ = false;
}

bool GPUCommandBufferVk::BeginRecording() {
  if (is_recording_) {
    return true;
  }

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VkResult result = vkBeginCommandBuffer(command_buffer_, &begin_info);
  if (result != VK_SUCCESS) {
    LOGE("Failed to begin command buffer recording: %d", result);
    return false;
  }

  is_recording_ = true;
  return true;
}

bool GPUCommandBufferVk::EndRecording() {
  if (!is_recording_) {
    return true;
  }

  VkResult result = vkEndCommandBuffer(command_buffer_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to end command buffer recording: %d", result);
    return false;
  }

  is_recording_ = false;
  return true;
}

std::shared_ptr<GPURenderPass> GPUCommandBufferVk::BeginRenderPass(
    const GPURenderPassDescriptor& desc) {
  if (!BeginRecording()) {
    LOGE("Failed to begin command buffer recording");
    return nullptr;
  }

  auto render_pass = std::make_shared<GPURenderPassVk>(this, desc);
  render_passes_.push_back(render_pass);
  return render_pass;
}

std::shared_ptr<GPUBlitPass> GPUCommandBufferVk::BeginBlitPass() {
  if (!BeginRecording()) {
    LOGE("Failed to begin command buffer recording");
    return nullptr;
  }

  auto blit_pass = std::make_shared<GPUBlitPassVk>(this);
  blit_passes_.push_back(blit_pass);
  return blit_pass;
}

bool GPUCommandBufferVk::Submit() {
  if (!EndRecording()) {
    LOGE("Failed to end command buffer recording");
    return false;
  }

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer_;

  VkQueue graphics_queue = device_->GetGraphicsQueue();
  VkResult result =
      vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
  if (result != VK_SUCCESS) {
    LOGE("Failed to submit command buffer: %d", result);
    return false;
  }

  // Wait for queue to complete (synchronous for now)
  vkQueueWaitIdle(graphics_queue);

  // Reset for next use
  Reset();

  return true;
}

void GPUCommandBufferVk::AddMemoryBarrier(const VkMemoryBarrier& barrier) {
  if (sync_manager_) {
    sync_manager_->AddMemoryBarrier(barrier);
  }
}

void GPUCommandBufferVk::AddImageBarrier(const VkImageBarrier& barrier) {
  if (sync_manager_) {
    sync_manager_->AddImageBarrier(barrier);
  }
}

void GPUCommandBufferVk::AddBufferBarrier(const VkBufferBarrier& barrier) {
  if (sync_manager_) {
    sync_manager_->AddBufferBarrier(barrier);
  }
}

void GPUCommandBufferVk::ExecuteBarriers() {
  if (sync_manager_ && command_buffer_ != VK_NULL_HANDLE) {
    sync_manager_->ExecuteBarriers(command_buffer_);
    sync_manager_->Reset();
  }
}

void GPUCommandBufferVk::TransitionImageLayout(VkImage image,
                                               VkImageLayout old_layout,
                                               VkImageLayout new_layout,
                                               VkImageAspectFlags aspect_mask) {
  if (!sync_manager_) {
    LOGW("No sync manager for image transition");
    return;
  }

  if (image == VK_NULL_HANDLE) {
    LOGE("Cannot transition null image layout");
    return;
  }

  auto barrier = VkSyncManager::CreateImageTransitionBarrier(
      image, old_layout, new_layout, aspect_mask);
  sync_manager_->AddImageBarrier(barrier);
  sync_manager_->ExecuteBarriers(command_buffer_);
  sync_manager_->Reset();
}

}  // namespace skity