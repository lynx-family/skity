// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_TEXTURE_VK_HPP
#define SRC_GPU_VK_GPU_TEXTURE_VK_HPP

#include <vk_mem_alloc.h>
#include <volk.h>

#include "src/gpu/backend_cast.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

class GPUDeviceVk;

class GPUTextureVk : public GPUTexture {
 public:
  explicit GPUTextureVk(const GPUTextureDescriptor& descriptor);
  ~GPUTextureVk() override;

  static std::shared_ptr<GPUTextureVk> Create(
      GPUDeviceVk* device, const GPUTextureDescriptor& descriptor);

  // Create a texture wrapper around an existing VkImage (e.g., swapchain image)
  static std::shared_ptr<GPUTextureVk> CreateFromVkImage(GPUDeviceVk* device,
                                                         VkImage vk_image,
                                                         VkFormat vk_format,
                                                         uint32_t width,
                                                         uint32_t height);

  bool Initialize(GPUDeviceVk* device);
  void Destroy();

  VkImage GetVkImage() const { return image_; }
  VkImageView GetVkImageView() const { return image_view_; }
  VkFormat GetVkFormat() const { return format_; }

  void UploadData(GPUDeviceVk* device, uint32_t offset_x, uint32_t offset_y,
                  uint32_t width, uint32_t height, const void* data);

  size_t GetBytes() const override;

  SKT_BACKEND_CAST(GPUTextureVk, GPUTexture)

 private:
  bool CreateImage(GPUDeviceVk* device);
  bool CreateImageView(GPUDeviceVk* device);
  void TransitionImageLayout(GPUDeviceVk* device, VkImageLayout old_layout,
                             VkImageLayout new_layout);
  VmaMemoryUsage GetOptimalMemoryUsage() const;

  VkImage image_ = VK_NULL_HANDLE;
  VkImageView image_view_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkImageLayout current_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  GPUDeviceVk* device_ = nullptr;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_TEXTURE_VK_HPP
