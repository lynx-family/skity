// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_sampler_vk.hpp"

#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/logging.hpp"

namespace skity {

GPUSamplerVk::GPUSamplerVk(const GPUSamplerDescriptor& descriptor)
    : GPUSampler(descriptor) {}

GPUSamplerVk::~GPUSamplerVk() { Destroy(); }

std::shared_ptr<GPUSamplerVk> GPUSamplerVk::Create(
    GPUDeviceVk* device, const GPUSamplerDescriptor& descriptor) {
  if (!device) {
    LOGE("Invalid device for sampler creation");
    return nullptr;
  }

  auto sampler = std::make_shared<GPUSamplerVk>(descriptor);
  if (!sampler->Initialize(device)) {
    LOGE("Failed to initialize Vulkan sampler");
    return nullptr;
  }

  return sampler;
}

bool GPUSamplerVk::Initialize(GPUDeviceVk* device) {
  if (!device) {
    LOGE("Invalid device for sampler initialization");
    return false;
  }

  device_ = device;

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = ConvertFilter(desc_.mag_filter);
  sampler_info.minFilter = ConvertFilter(desc_.min_filter);
  sampler_info.addressModeU = ConvertAddressMode(desc_.address_mode_u);
  sampler_info.addressModeV = ConvertAddressMode(desc_.address_mode_v);
  sampler_info.addressModeW = ConvertAddressMode(desc_.address_mode_w);
  sampler_info.anisotropyEnable = VK_FALSE;
  sampler_info.maxAnisotropy = 1.0f;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_info.mipmapMode = ConvertMipmapMode(desc_.mipmap_filter);
  sampler_info.mipLodBias = 0.0f;
  sampler_info.minLod = 0.0f;
  sampler_info.maxLod = VK_LOD_CLAMP_NONE;

  VkResult result =
      vkCreateSampler(device_->GetDevice(), &sampler_info, nullptr, &sampler_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create Vulkan sampler: %d", result);
    return false;
  }

  return true;
}

void GPUSamplerVk::Destroy() {
  if (sampler_ != VK_NULL_HANDLE && device_) {
    vkDestroySampler(device_->GetDevice(), sampler_, nullptr);
    sampler_ = VK_NULL_HANDLE;
  }
  device_ = nullptr;
}

VkFilter GPUSamplerVk::ConvertFilter(GPUFilterMode filter) const {
  switch (filter) {
    case GPUFilterMode::kNearest:
      return VK_FILTER_NEAREST;
    case GPUFilterMode::kLinear:
      return VK_FILTER_LINEAR;
    default:
      return VK_FILTER_NEAREST;
  }
}

VkSamplerMipmapMode GPUSamplerVk::ConvertMipmapMode(
    GPUMipmapMode mipmap_mode) const {
  switch (mipmap_mode) {
    case GPUMipmapMode::kNone:
      // When no mipmapping, use nearest for best performance
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case GPUMipmapMode::kNearest:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case GPUMipmapMode::kLinear:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }
}

VkSamplerAddressMode GPUSamplerVk::ConvertAddressMode(
    GPUAddressMode address_mode) const {
  switch (address_mode) {
    case GPUAddressMode::kClampToEdge:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case GPUAddressMode::kRepeat:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case GPUAddressMode::kMirrorRepeat:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    default:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
}

}  // namespace skity
