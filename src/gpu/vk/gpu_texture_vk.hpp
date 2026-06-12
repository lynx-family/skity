// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_TEXTURE_VK_HPP
#define SRC_GPU_VK_GPU_TEXTURE_VK_HPP

#include <vk_mem_alloc.h>

#include <memory>

#include "src/gpu/backend_cast.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

class VulkanContextState;

/**
 * Returns the VkImageAspectFlags for an image memory barrier based on the
 * actual VkFormat. For combined depth/stencil formats (e.g. D24_UNORM_S8_UINT),
 * always returns both DEPTH | STENCIL aspects, as required by the Vulkan spec
 * when the separateDepthStencilLayouts feature is not enabled.
 */
inline VkImageAspectFlags VkFormatAspectMaskForBarrier(VkFormat format) {
  switch (format) {
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
      return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_S8_UINT:
      return VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

class GPUTextureVK : public GPUTexture,
                     public std::enable_shared_from_this<GPUTextureVK> {
 public:
  GPUTextureVK(std::shared_ptr<const VulkanContextState> state,
               const GPUTextureDescriptor& descriptor, VkImage image,
               VmaAllocation allocation, VkImageView image_view,
               VkImageLayout preferred_layout, VkFormat format,
               bool owns_image = true, bool owns_image_view = true);

  ~GPUTextureVK() override;

  static std::shared_ptr<GPUTexture> Create(
      std::shared_ptr<const VulkanContextState> state,
      const GPUTextureDescriptor& descriptor);

  static std::shared_ptr<GPUTexture> Wrap(
      std::shared_ptr<const VulkanContextState> state,
      const GPUTextureDescriptor& descriptor, VkImage image,
      VkImageView image_view, VkImageLayout initial_layout,
      VkImageLayout preferred_layout, VkFormat format, bool owns_image,
      bool owns_image_view);

  size_t GetBytes() const override;

  void UploadData(uint32_t offset_x, uint32_t offset_y, uint32_t width,
                  uint32_t height, void* data) override;

  VkImage GetImage() const { return image_; }

  VkImageView GetImageView() const { return image_view_; }

  VkFormat GetVkFormat() const { return format_; }

  static VkFormat ToVkFormat(GPUTextureFormat format);

  VkImageLayout GetPreferredLayout() const { return preferred_layout_; }

  VkImageLayout GetCurrentLayout() const { return current_layout_; }

  void SetCurrentLayout(VkImageLayout layout) { current_layout_ = layout; }

  bool IsValid() const {
    return image_ != VK_NULL_HANDLE && image_view_ != VK_NULL_HANDLE;
  }

  SKT_BACKEND_CAST(GPUTextureVK, GPUTexture)

 private:
  std::shared_ptr<const VulkanContextState> state_ = {};
  VkImage image_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = nullptr;
  VkImageView image_view_ = VK_NULL_HANDLE;
  VkImageLayout preferred_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageLayout current_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  bool owns_image_ = true;
  bool owns_image_view_ = true;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_TEXTURE_VK_HPP
