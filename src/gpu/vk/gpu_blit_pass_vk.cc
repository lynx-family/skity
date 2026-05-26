// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_blit_pass_vk.hpp"

#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

VkImageAspectFlags GetImageAspectMask(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kStencil8:
      return VK_IMAGE_ASPECT_STENCIL_BIT;
    case GPUTextureFormat::kDepth24Stencil8:
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case GPUTextureFormat::kInvalid:
      return 0;
    default:
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

VkAccessFlags AccessMaskForLayout(VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
      return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
             VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    default:
      return 0;
  }
}

VkPipelineStageFlags StageMaskForLayout(VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
      return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    default:
      return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }
}

void TransitionImageLayout(const VulkanContextState& state,
                           VkCommandBuffer command_buffer,
                           GPUTextureVK& texture, VkImageLayout new_layout) {
  const VkImageLayout old_layout = texture.GetCurrentLayout();
  if (old_layout == new_layout) {
    return;
  }

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = texture.GetImage();
  barrier.subresourceRange.aspectMask =
      GetImageAspectMask(texture.GetDescriptor().format);
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = texture.GetDescriptor().mip_level_count;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = AccessMaskForLayout(old_layout);
  barrier.dstAccessMask = AccessMaskForLayout(new_layout);

  state.DeviceFns().vkCmdPipelineBarrier(
      command_buffer, StageMaskForLayout(old_layout),
      StageMaskForLayout(new_layout), 0, 0, nullptr, 0, nullptr, 1, &barrier);

  texture.SetCurrentLayout(new_layout);
}

void TransitionMipRangeLayout(const VulkanContextState& state,
                              VkCommandBuffer command_buffer,
                              GPUTextureVK& texture, uint32_t base_mip_level,
                              uint32_t level_count, VkImageLayout old_layout,
                              VkImageLayout new_layout) {
  if (old_layout == new_layout || level_count == 0) {
    return;
  }

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = texture.GetImage();
  barrier.subresourceRange.aspectMask =
      GetImageAspectMask(texture.GetDescriptor().format);
  barrier.subresourceRange.baseMipLevel = base_mip_level;
  barrier.subresourceRange.levelCount = level_count;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = AccessMaskForLayout(old_layout);
  barrier.dstAccessMask = AccessMaskForLayout(new_layout);

  state.DeviceFns().vkCmdPipelineBarrier(
      command_buffer, StageMaskForLayout(old_layout),
      StageMaskForLayout(new_layout), 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool SupportsLinearBlit(const VulkanContextState& state, VkFormat format) {
  if (format == VK_FORMAT_UNDEFINED ||
      state.InstanceFns().vkGetPhysicalDeviceFormatProperties == nullptr) {
    return false;
  }

  VkFormatProperties format_properties = {};
  state.InstanceFns().vkGetPhysicalDeviceFormatProperties(
      state.GetPhysicalDevice(), format, &format_properties);
  return (format_properties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
}

}  // namespace

GPUBlitPassVK::GPUBlitPassVK(std::shared_ptr<const VulkanContextState> state,
                             GPUCommandBufferVK* command_buffer)
    : state_(std::move(state)), command_buffer_(command_buffer) {}

void GPUBlitPassVK::UploadTextureData(std::shared_ptr<GPUTexture> texture,
                                      uint32_t offset_x, uint32_t offset_y,
                                      uint32_t width, uint32_t height,
                                      void* data) {
  const auto texture_ref = texture;
  auto texture_vk = GPUTextureVK::Cast(texture.get());
  if (texture_vk == nullptr || state_ == nullptr ||
      command_buffer_ == nullptr || data == nullptr || width == 0 ||
      height == 0) {
    LOGE("Failed to upload Vulkan texture data: invalid upload state");
    return;
  }

  if (texture_vk->GetImage() == VK_NULL_HANDLE ||
      command_buffer_->GetCommandBuffer() == VK_NULL_HANDLE) {
    LOGE(
        "Failed to upload Vulkan texture data: image or command buffer is "
        "unavailable");
    return;
  }

  if (texture_vk->GetDescriptor().sample_count != 1) {
    LOGW("Uploading CPU data to multisampled Vulkan textures is unsupported");
    return;
  }

  const size_t bytes_per_pixel =
      GetTextureFormatBytesPerPixel(texture_vk->GetDescriptor().format);
  if (bytes_per_pixel == 0) {
    LOGE("Failed to upload Vulkan texture data: invalid texture format");
    return;
  }

  const size_t upload_size =
      static_cast<size_t>(width) * height * bytes_per_pixel;
  auto staging_buffer = std::make_unique<GPUBufferVK>(
      0u, state_, GPUBufferVKMemoryType::kHostVisible);
  if (!staging_buffer->UploadData(data, upload_size)) {
    return;
  }

  TransitionImageLayout(*state_, command_buffer_->GetCommandBuffer(),
                        *texture_vk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkBufferImageCopy copy_region = {};
  copy_region.bufferOffset = 0;
  copy_region.bufferRowLength = 0;
  copy_region.bufferImageHeight = 0;
  copy_region.imageSubresource.aspectMask =
      GetImageAspectMask(texture_vk->GetDescriptor().format);
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageOffset = {static_cast<int32_t>(offset_x),
                             static_cast<int32_t>(offset_y), 0};
  copy_region.imageExtent = {width, height, 1};

  state_->DeviceFns().vkCmdCopyBufferToImage(
      command_buffer_->GetCommandBuffer(), staging_buffer->GetBuffer(),
      texture_vk->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
      &copy_region);

  TransitionImageLayout(*state_, command_buffer_->GetCommandBuffer(),
                        *texture_vk, texture_vk->GetPreferredLayout());
  command_buffer_->RecordStageBuffer(std::move(staging_buffer));
  command_buffer_->RecordCleanupAction([texture_ref]() {});
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
  const auto texture_ref = texture;
  auto* texture_vk = GPUTextureVK::Cast(texture.get());
  if (texture_vk == nullptr || state_ == nullptr ||
      command_buffer_ == nullptr) {
    LOGE("Failed to generate Vulkan mipmaps: invalid blit pass state");
    return;
  }

  if (texture_vk->GetImage() == VK_NULL_HANDLE ||
      command_buffer_->GetCommandBuffer() == VK_NULL_HANDLE) {
    LOGE(
        "Failed to generate Vulkan mipmaps: image or command buffer is "
        "unavailable");
    return;
  }

  const auto& desc = texture_vk->GetDescriptor();
  if (desc.mip_level_count <= 1) {
    return;
  }

  if (desc.sample_count != 1) {
    LOGW("Skipping Vulkan mipmap generation for multisampled texture");
    return;
  }

  if (GetImageAspectMask(desc.format) != VK_IMAGE_ASPECT_COLOR_BIT) {
    LOGW("Skipping Vulkan mipmap generation for non-color texture format");
    return;
  }

  if (!SupportsLinearBlit(*state_, texture_vk->GetVkFormat())) {
    LOGW(
        "Skipping Vulkan mipmap generation: format does not support linear "
        "blit");
    return;
  }

  const VkCommandBuffer command_buffer = command_buffer_->GetCommandBuffer();
  const VkImageLayout initial_layout = texture_vk->GetCurrentLayout();

  TransitionMipRangeLayout(*state_, command_buffer, *texture_vk, 0, 1,
                           initial_layout,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  TransitionMipRangeLayout(*state_, command_buffer, *texture_vk, 1,
                           desc.mip_level_count - 1, initial_layout,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  int32_t mip_width = static_cast<int32_t>(desc.width);
  int32_t mip_height = static_cast<int32_t>(desc.height);

  for (uint32_t mip_level = 1; mip_level < desc.mip_level_count; ++mip_level) {
    const int32_t next_width = std::max(1, mip_width / 2);
    const int32_t next_height = std::max(1, mip_height / 2);

    VkImageBlit blit_region = {};
    blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.srcSubresource.mipLevel = mip_level - 1;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount = 1;
    blit_region.srcOffsets[0] = {0, 0, 0};
    blit_region.srcOffsets[1] = {mip_width, mip_height, 1};
    blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.dstSubresource.mipLevel = mip_level;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount = 1;
    blit_region.dstOffsets[0] = {0, 0, 0};
    blit_region.dstOffsets[1] = {next_width, next_height, 1};

    state_->DeviceFns().vkCmdBlitImage(command_buffer, texture_vk->GetImage(),
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       texture_vk->GetImage(),
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                       &blit_region, VK_FILTER_LINEAR);

    TransitionMipRangeLayout(
        *state_, command_buffer, *texture_vk, mip_level - 1, 1,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture_vk->GetPreferredLayout());

    if (mip_level < desc.mip_level_count - 1) {
      TransitionMipRangeLayout(*state_, command_buffer, *texture_vk, mip_level,
                               1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    } else {
      TransitionMipRangeLayout(*state_, command_buffer, *texture_vk, mip_level,
                               1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               texture_vk->GetPreferredLayout());
    }

    mip_width = next_width;
    mip_height = next_height;
  }

  texture_vk->SetCurrentLayout(texture_vk->GetPreferredLayout());
  command_buffer_->RecordCleanupAction([texture_ref]() {});
}

void GPUBlitPassVK::CopyTextureToTexture(std::shared_ptr<GPUTexture> src,
                                         std::shared_ptr<GPUTexture> dst,
                                         const TextureCopyRegion& region) {
  if (!src || !dst) {
    LOGE("CopyTextureToTexture called with empty src or dst");
    return;
  }

  if (region.width == 0 || region.height == 0) {
    LOGE("CopyTextureToTexture called with empty width or height");
    return;
  }

  auto* src_vk = GPUTextureVK::Cast(src.get());
  auto* dst_vk = GPUTextureVK::Cast(dst.get());

  if (!src_vk || !dst_vk || !state_ || !command_buffer_) {
    LOGE("CopyTextureToTexture: invalid state");
    return;
  }

  if (src_vk->GetImage() == VK_NULL_HANDLE ||
      dst_vk->GetImage() == VK_NULL_HANDLE ||
      command_buffer_->GetCommandBuffer() == VK_NULL_HANDLE) {
    LOGE("CopyTextureToTexture: image or command buffer unavailable");
    return;
  }

  const auto src_ref = src;
  const auto dst_ref = dst;
  const VkCommandBuffer cmd_buf = command_buffer_->GetCommandBuffer();

  TransitionImageLayout(*state_, cmd_buf, *src_vk,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  TransitionImageLayout(*state_, cmd_buf, *dst_vk,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkImageCopy copy_region = {};
  copy_region.srcSubresource.aspectMask =
      GetImageAspectMask(src_vk->GetDescriptor().format);
  copy_region.srcSubresource.mipLevel = 0;
  copy_region.srcSubresource.baseArrayLayer = 0;
  copy_region.srcSubresource.layerCount = 1;
  copy_region.srcOffset = {static_cast<int32_t>(region.src_x),
                           static_cast<int32_t>(region.src_y), 0};
  copy_region.dstSubresource.aspectMask =
      GetImageAspectMask(dst_vk->GetDescriptor().format);
  copy_region.dstSubresource.mipLevel = 0;
  copy_region.dstSubresource.baseArrayLayer = 0;
  copy_region.dstSubresource.layerCount = 1;
  copy_region.dstOffset = {static_cast<int32_t>(region.dst_x),
                           static_cast<int32_t>(region.dst_y), 0};
  copy_region.extent = {region.width, region.height, 1};

  state_->DeviceFns().vkCmdCopyImage(
      cmd_buf, src_vk->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      dst_vk->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
      &copy_region);

  TransitionImageLayout(*state_, cmd_buf, *src_vk,
                        src_vk->GetPreferredLayout());
  TransitionImageLayout(*state_, cmd_buf, *dst_vk,
                        dst_vk->GetPreferredLayout());
  command_buffer_->RecordCleanupAction([src_ref, dst_ref]() {});
}

void GPUBlitPassVK::End() { InsertDebugLabelIfNeeded(); }

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
