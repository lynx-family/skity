// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_texture_vk.hpp"

#include "src/gpu/vk/gpu_blit_pass_vk.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

VkFormat ConvertToVkFormat(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kR8Unorm:
      return VK_FORMAT_R8_UNORM;
    case GPUTextureFormat::kRGB8Unorm:
      return VK_FORMAT_R8G8B8_UNORM;
    case GPUTextureFormat::kRGB565Unorm:
      return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case GPUTextureFormat::kRGBA8Unorm:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case GPUTextureFormat::kBGRA8Unorm:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case GPUTextureFormat::kStencil8:
      return VK_FORMAT_S8_UINT;
    case GPUTextureFormat::kDepth24Stencil8:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case GPUTextureFormat::kInvalid:
      return VK_FORMAT_UNDEFINED;
  }

  return VK_FORMAT_UNDEFINED;
}

VkImageUsageFlags ToVkImageUsage(GPUTextureUsageMask usage) {
  VkImageUsageFlags flags = 0;

  if ((usage & static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopySrc)) !=
      0) {
    flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  if ((usage & static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopyDst)) !=
      0) {
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  if ((usage & static_cast<GPUTextureUsageMask>(
                   GPUTextureUsage::kTextureBinding)) != 0) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  if ((usage & static_cast<GPUTextureUsageMask>(
                   GPUTextureUsage::kStorageBinding)) != 0) {
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }

  if ((usage & static_cast<GPUTextureUsageMask>(
                   GPUTextureUsage::kRenderAttachment)) != 0) {
    flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }

  return flags;
}

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

VkImageUsageFlags SanitizeImageUsage(GPUTextureFormat format,
                                     VkImageUsageFlags usage) {
  const bool is_depth_stencil = format == GPUTextureFormat::kStencil8 ||
                                format == GPUTextureFormat::kDepth24Stencil8;

  if (is_depth_stencil) {
    usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  } else {
    usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }

  return usage;
}

bool IsImageFormatSupported(const VulkanContextState& state, VkFormat format,
                            VkImageUsageFlags usage,
                            VkSampleCountFlagBits samples) {
  if (format == VK_FORMAT_UNDEFINED ||
      state.InstanceFns().vkGetPhysicalDeviceImageFormatProperties == nullptr) {
    return false;
  }

  VkImageFormatProperties properties = {};
  const VkResult result =
      state.InstanceFns().vkGetPhysicalDeviceImageFormatProperties(
          state.GetPhysicalDevice(), format, VK_IMAGE_TYPE_2D,
          VK_IMAGE_TILING_OPTIMAL, usage, 0, &properties);
  if (result != VK_SUCCESS) {
    return false;
  }

  return (properties.sampleCounts & samples) != 0;
}

VkFormat ResolveVkFormat(const VulkanContextState& state,
                         GPUTextureFormat texture_format,
                         VkImageUsageFlags usage,
                         VkSampleCountFlagBits samples) {
  const VkFormat preferred = ConvertToVkFormat(texture_format);
  if (IsImageFormatSupported(state, preferred, usage, samples)) {
    return preferred;
  }

  if (texture_format == GPUTextureFormat::kDepth24Stencil8 &&
      IsImageFormatSupported(state, VK_FORMAT_D32_SFLOAT_S8_UINT, usage,
                             samples)) {
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
  }

  return VK_FORMAT_UNDEFINED;
}

VkImageLayout ResolvePreferredLayout(const GPUTextureDescriptor& descriptor) {
  const bool render_attachment =
      (descriptor.usage & static_cast<GPUTextureUsageMask>(
                              GPUTextureUsage::kRenderAttachment)) != 0;
  const bool texture_binding =
      (descriptor.usage &
       static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding)) != 0;
  const bool storage_binding =
      (descriptor.usage &
       static_cast<GPUTextureUsageMask>(GPUTextureUsage::kStorageBinding)) != 0;
  const bool multi_use = (render_attachment && texture_binding) ||
                         storage_binding || descriptor.sample_count > 1 ||
                         descriptor.mip_level_count > 1;

  if (multi_use) {
    return VK_IMAGE_LAYOUT_GENERAL;
  }

  if (render_attachment) {
    if (descriptor.format == GPUTextureFormat::kStencil8 ||
        descriptor.format == GPUTextureFormat::kDepth24Stencil8) {
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  if (texture_binding) {
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  if ((descriptor.usage &
       static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopySrc)) != 0) {
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  }

  if ((descriptor.usage &
       static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopyDst)) != 0) {
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  }

  return VK_IMAGE_LAYOUT_GENERAL;
}

}  // namespace

VkFormat GPUTextureVK::ToVkFormat(GPUTextureFormat format) {
  return ConvertToVkFormat(format);
}

GPUTextureVK::GPUTextureVK(std::shared_ptr<const VulkanContextState> state,
                           const GPUTextureDescriptor& descriptor,
                           VkImage image, VmaAllocation allocation,
                           VkImageView image_view,
                           VkImageLayout preferred_layout, VkFormat format)
    : GPUTexture(descriptor),
      state_(std::move(state)),
      image_(image),
      allocation_(allocation),
      image_view_(image_view),
      preferred_layout_(preferred_layout),
      format_(format) {}

GPUTextureVK::~GPUTextureVK() {
  if (state_ != nullptr && state_->GetLogicalDevice() != VK_NULL_HANDLE &&
      image_view_ != VK_NULL_HANDLE &&
      state_->DeviceFns().vkDestroyImageView != nullptr) {
    state_->DeviceFns().vkDestroyImageView(state_->GetLogicalDevice(),
                                           image_view_, nullptr);
  }

  if (state_ != nullptr && state_->GetAllocator() != nullptr &&
      image_ != VK_NULL_HANDLE && allocation_ != VK_NULL_HANDLE) {
    vmaDestroyImage(state_->GetAllocator(), image_, allocation_);
  }
}

std::shared_ptr<GPUTexture> GPUTextureVK::Create(
    std::shared_ptr<const VulkanContextState> state,
    const GPUTextureDescriptor& descriptor) {
  if (state == nullptr || state->GetLogicalDevice() == VK_NULL_HANDLE ||
      state->GetAllocator() == nullptr) {
    LOGE("Failed to create Vulkan texture: device or allocator is unavailable");
    return {};
  }

  const VkFormat requested_format = ConvertToVkFormat(descriptor.format);
  if (requested_format == VK_FORMAT_UNDEFINED || descriptor.width == 0 ||
      descriptor.height == 0) {
    LOGE("Failed to create Vulkan texture: invalid descriptor");
    return {};
  }

  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = descriptor.width;
  image_info.extent.height = descriptor.height;
  image_info.extent.depth = 1;
  image_info.mipLevels = descriptor.mip_level_count;
  image_info.arrayLayers = 1;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.samples =
      static_cast<VkSampleCountFlagBits>(descriptor.sample_count);
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.usage =
      SanitizeImageUsage(descriptor.format, ToVkImageUsage(descriptor.usage));
  if (descriptor.storage_mode == GPUTextureStorageMode::kMemoryless) {
    image_info.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
  }

  if (image_info.usage == 0) {
    LOGE("Failed to create Vulkan texture: empty image usage");
    return {};
  }

  image_info.format = ResolveVkFormat(*state, descriptor.format,
                                      image_info.usage, image_info.samples);
  if (image_info.format == VK_FORMAT_UNDEFINED) {
    LOGE("Failed to create Vulkan texture: unsupported format");
    return {};
  }

  VmaAllocationCreateInfo allocation_info = {};
  if (descriptor.storage_mode == GPUTextureStorageMode::kMemoryless) {
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocation_info.requiredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
  } else if (descriptor.storage_mode == GPUTextureStorageMode::kHostVisible) {
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  } else {
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  }

  VkImage image = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  const VkResult create_result =
      vmaCreateImage(state->GetAllocator(), &image_info, &allocation_info,
                     &image, &allocation, nullptr);
  if (create_result != VK_SUCCESS || image == VK_NULL_HANDLE ||
      allocation == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan image {}x{}: {}", descriptor.width,
         descriptor.height, static_cast<int32_t>(create_result));
    return {};
  }

  VkImageViewCreateInfo view_info = {};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = image_info.format;
  view_info.subresourceRange.aspectMask = GetImageAspectMask(descriptor.format);
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = descriptor.mip_level_count;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  VkImageView image_view = VK_NULL_HANDLE;
  const VkResult view_result = state->DeviceFns().vkCreateImageView(
      state->GetLogicalDevice(), &view_info, nullptr, &image_view);
  if (view_result != VK_SUCCESS || image_view == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan image view: {}",
         static_cast<int32_t>(view_result));
    vmaDestroyImage(state->GetAllocator(), image, allocation);
    return {};
  }

  return std::make_shared<GPUTextureVK>(
      state, descriptor, image, allocation, image_view,
      ResolvePreferredLayout(descriptor), image_info.format);
}

size_t GPUTextureVK::GetBytes() const {
  const auto& desc = GetDescriptor();
  if (desc.storage_mode == GPUTextureStorageMode::kMemoryless) {
    return 0;
  }

  return static_cast<size_t>(desc.width) * desc.height *
         GetTextureFormatBytesPerPixel(desc.format) * desc.sample_count;
}

void GPUTextureVK::UploadData(uint32_t offset_x, uint32_t offset_y,
                              uint32_t width, uint32_t height, void* data) {
  if (state_ == nullptr || data == nullptr || width == 0 || height == 0) {
    return;
  }

  auto command_buffer = std::make_shared<GPUCommandBufferVK>(state_);
  if (!command_buffer->Init()) {
    return;
  }

  auto blit_pass =
      std::make_shared<GPUBlitPassVK>(state_, command_buffer.get());
  blit_pass->UploadTextureData(shared_from_this(), offset_x, offset_y, width,
                               height, data);
  blit_pass->End();
  command_buffer->Submit();
}

}  // namespace skity
