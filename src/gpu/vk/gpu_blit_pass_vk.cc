// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_blit_pass_vk.hpp"

#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

GPUBlitPassVK::GPUBlitPassVK(std::shared_ptr<const VulkanContextState> state,
                             GPUCommandBufferVK* command_buffer)
    : state_(std::move(state)), command_buffer_(command_buffer) {}

void GPUBlitPassVK::UploadTextureData(std::shared_ptr<GPUTexture> texture,
                                      uint32_t offset_x, uint32_t offset_y,
                                      uint32_t width, uint32_t height,
                                      void* data) {
  (void)texture;
  (void)offset_x;
  (void)offset_y;
  (void)width;
  (void)height;
  (void)data;
  LOGW("GPUBlitPassVK::UploadTextureData is not implemented yet");
}

void GPUBlitPassVK::UploadBufferData(GPUBuffer* buffer, void* data,
                                     size_t size) {
  auto* destination_buffer = static_cast<GPUBufferVK*>(buffer);
  if (destination_buffer == nullptr || state_ == nullptr ||
      command_buffer_ == nullptr) {
    LOGE("Failed to upload Vulkan buffer data: invalid upload state");
    return;
  }

  if (!destination_buffer->ResizeIfNeeded(size)) {
    return;
  }

  auto staging_buffer = std::make_unique<GPUBufferVK>(
      0u, state_, GPUBufferVKMemoryType::kHostVisible);
  if (!staging_buffer->UploadData(data, size)) {
    return;
  }

  VkBufferCopy copy_region = {};
  copy_region.size = size;

  state_->DeviceFns().vkCmdCopyBuffer(
      command_buffer_->GetCommandBuffer(), staging_buffer->GetBuffer(),
      destination_buffer->GetBuffer(), 1, &copy_region);

  command_buffer_->RecordStageBuffer(std::move(staging_buffer));
}

void GPUBlitPassVK::GenerateMipmaps(
    const std::shared_ptr<GPUTexture>& texture) {
  (void)texture;
  LOGW("GPUBlitPassVK::GenerateMipmaps is not implemented yet");
}

void GPUBlitPassVK::End() {
  InsertDebugLabelIfNeeded();
}

void GPUBlitPassVK::InsertDebugLabelIfNeeded() {
#if defined(SKITY_VK_DEBUG_RUNTIME)
  if (state_ == nullptr || command_buffer_ == nullptr ||
      !state_->HasEnabledInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) ||
      state_->DeviceFns().vkCmdInsertDebugUtilsLabelEXT == nullptr) {
    return;
  }

  std::string label = command_buffer_->GetLabel();
  if (label.empty()) {
    label = "BlitPass";
  } else {
    label += " BlitPass";
  }

  VkDebugUtilsLabelEXT debug_label = {};
  debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  debug_label.pLabelName = label.c_str();

  state_->DeviceFns().vkCmdInsertDebugUtilsLabelEXT(
      command_buffer_->GetCommandBuffer(), &debug_label);
#endif
}

}  // namespace skity
