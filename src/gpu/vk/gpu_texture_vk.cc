// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_texture_vk.hpp"

#include <algorithm>
#include <cstring>

#include "src/gpu/vk/formats_vk.h"
#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/sync_objects_vk.hpp"
#include "src/logging.hpp"

namespace skity {

GPUTextureVk::GPUTextureVk(const GPUTextureDescriptor& descriptor)
    : GPUTexture(descriptor) {}

GPUTextureVk::~GPUTextureVk() { Destroy(); }

std::shared_ptr<GPUTextureVk> GPUTextureVk::Create(
    GPUDeviceVk* device, const GPUTextureDescriptor& descriptor) {
  if (!device) {
    LOGE("Invalid device for texture creation");
    return nullptr;
  }

  auto texture = std::make_shared<GPUTextureVk>(descriptor);
  if (!texture->Initialize(device)) {
    LOGE("Failed to initialize Vulkan texture");
    return nullptr;
  }

  return texture;
}

std::shared_ptr<GPUTextureVk> GPUTextureVk::CreateFromVkImage(
    GPUDeviceVk* device, VkImage vk_image, VkFormat vk_format, uint32_t width,
    uint32_t height) {
  if (!device || vk_image == VK_NULL_HANDLE) {
    LOGE("Invalid device or VkImage for texture wrapping");
    return nullptr;
  }

  // Create a descriptor for the wrapper texture
  GPUTextureDescriptor desc;
  desc.width = width;
  desc.height = height;
  // Map the actual swapchain format correctly
  if (vk_format == VK_FORMAT_B8G8R8A8_SRGB) {
    desc.format = GPUTextureFormat::kBGRA8Unorm;  // Match swapchain format
  } else {
    desc.format = GPUTextureFormat::kRGBA8Unorm;  // Fallback
  }
  desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);

  auto texture = std::make_shared<GPUTextureVk>(desc);
  texture->device_ = device;
  texture->format_ = vk_format;
  texture->image_ = vk_image;             // Use the provided VkImage
  texture->allocation_ = VK_NULL_HANDLE;  // No allocation for external images

  // Only create the image view for the wrapped image
  if (!texture->CreateImageView(device)) {
    LOGE("Failed to create image view for wrapped VkImage");
    return nullptr;
  }

  return texture;
}

bool GPUTextureVk::Initialize(GPUDeviceVk* device) {
  if (!device) {
    LOGE("Invalid device for texture initialization");
    return false;
  }

  device_ = device;
  format_ = GPUTextureFormatToVkFormat(desc_.format);

  if (format_ == VK_FORMAT_UNDEFINED) {
    LOGE("Unsupported texture format");
    return false;
  }

  if (!CreateImage(device)) {
    LOGE("Failed to create Vulkan image");
    return false;
  }

  if (!CreateImageView(device)) {
    LOGE("Failed to create Vulkan image view");
    Destroy();
    return false;
  }

  return true;
}

bool GPUTextureVk::CreateImage(GPUDeviceVk* device) {
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = desc_.width;
  image_info.extent.height = desc_.height;
  image_info.extent.depth = 1;
  image_info.mipLevels = desc_.mip_level_count;
  image_info.arrayLayers = 1;
  image_info.format = format_;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = GPUTextureUsageToVkImageUsage(desc_.usage, desc_.format);
  image_info.samples = static_cast<VkSampleCountFlagBits>(desc_.sample_count);
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = GetOptimalMemoryUsage();

  VkResult result = vmaCreateImage(device->GetAllocator(), &image_info,
                                   &alloc_info, &image_, &allocation_, nullptr);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create Vulkan image: %d", result);
    return false;
  }

  current_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  return true;
}

bool GPUTextureVk::CreateImageView(GPUDeviceVk* device) {
  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image_;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = format_;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = desc_.mip_level_count;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  // Handle depth/stencil formats
  if (desc_.format == GPUTextureFormat::kStencil8) {
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
  } else if (desc_.format == GPUTextureFormat::kDepth24Stencil8) {
    view_info.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  }

  VkResult result =
      vkCreateImageView(device->GetDevice(), &view_info, nullptr, &image_view_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create image view: %d", result);
    return false;
  }

  return true;
}

void GPUTextureVk::TransitionImageLayout(GPUDeviceVk* device,
                                         VkImageLayout old_layout,
                                         VkImageLayout new_layout) {
  VkCommandBuffer command_buffer = device->BeginSingleTimeCommands();

  // Use the new synchronization system for better barrier management
  VkSyncManager sync_manager(device);

  auto barrier = VkSyncManager::CreateImageTransitionBarrier(
      image_, old_layout, new_layout, VK_IMAGE_ASPECT_COLOR_BIT);

  // Set proper subresource range for this texture
  barrier.subresource_range.baseMipLevel = 0;
  barrier.subresource_range.levelCount = desc_.mip_level_count;
  barrier.subresource_range.baseArrayLayer = 0;
  barrier.subresource_range.layerCount = 1;

  sync_manager.AddImageBarrier(barrier);
  sync_manager.ExecuteBarriers(command_buffer);

  device->EndSingleTimeCommands(command_buffer);
  current_layout_ = new_layout;
}

void GPUTextureVk::UploadData(GPUDeviceVk* device, uint32_t offset_x,
                              uint32_t offset_y, uint32_t width,
                              uint32_t height, const void* data) {
  if (!data || !device) {
    LOGE("Invalid parameters for texture data upload");
    return;
  }

  size_t data_size =
      width * height * GetTextureFormatBytesPerPixel(desc_.format);

  // Create staging buffer
  auto staging_buffer_unique =
      device->CreateBuffer(GPUBufferUsage::kVertexBuffer);
  if (!staging_buffer_unique) {
    LOGE("Failed to create staging buffer for texture upload");
    return;
  }
  auto* staging_buffer = static_cast<GPUBufferVk*>(staging_buffer_unique.get());

  // Upload data to staging buffer
  staging_buffer->UploadData(const_cast<void*>(data), data_size);

  // Transition image layout for transfer
  TransitionImageLayout(device, current_layout_,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // Copy buffer to image
  VkCommandBuffer command_buffer = device->BeginSingleTimeCommands();

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {static_cast<int32_t>(offset_x),
                        static_cast<int32_t>(offset_y), 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(command_buffer, staging_buffer->GetBuffer(), image_,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  device->EndSingleTimeCommands(command_buffer);

  // Transition to shader read optimal layout
  TransitionImageLayout(device, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

size_t GPUTextureVk::GetBytes() const {
  return desc_.width * desc_.height *
         GetTextureFormatBytesPerPixel(desc_.format) * desc_.mip_level_count;
}

void GPUTextureVk::Destroy() {
  if (device_) {
    VkDevice vk_device = device_->GetDevice();

    if (image_view_ != VK_NULL_HANDLE) {
      vkDestroyImageView(vk_device, image_view_, nullptr);
      image_view_ = VK_NULL_HANDLE;
    }

    if (image_ != VK_NULL_HANDLE && allocation_ != VK_NULL_HANDLE) {
      vmaDestroyImage(device_->GetAllocator(), image_, allocation_);
      image_ = VK_NULL_HANDLE;
      allocation_ = VK_NULL_HANDLE;
    }
  }

  device_ = nullptr;
  current_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
}

VmaMemoryUsage GPUTextureVk::GetOptimalMemoryUsage() const {
  auto usage = desc_.usage;
  auto storage_mode = desc_.storage_mode;

  // Optimize memory usage based on texture usage and storage mode
  if (storage_mode == GPUTextureStorageMode::kPrivate) {
    return VMA_MEMORY_USAGE_GPU_ONLY;  // GPU-only for private textures
  }

  if (usage &
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment)) {
    return VMA_MEMORY_USAGE_GPU_ONLY;  // Render targets stay on GPU
  }

  if ((usage &
       static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding)) &&
      storage_mode == GPUTextureStorageMode::kHostVisible) {
    // Textures that might need CPU access for updates
    return VMA_MEMORY_USAGE_CPU_TO_GPU;
  }

  // Default: GPU-only for most textures
  return VMA_MEMORY_USAGE_GPU_ONLY;
}

}  // namespace skity
