// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_SAMPLER_VK_HPP
#define SRC_GPU_VK_GPU_SAMPLER_VK_HPP

#include <volk.h>

#include "src/gpu/backend_cast.hpp"
#include "src/gpu/gpu_sampler.hpp"

namespace skity {

class GPUDeviceVk;

class GPUSamplerVk : public GPUSampler {
 public:
  explicit GPUSamplerVk(const GPUSamplerDescriptor& descriptor);
  ~GPUSamplerVk() override;

  static std::shared_ptr<GPUSamplerVk> Create(
      GPUDeviceVk* device, const GPUSamplerDescriptor& descriptor);

  bool Initialize(GPUDeviceVk* device);
  void Destroy();

  VkSampler GetVkSampler() const { return sampler_; }

  SKT_BACKEND_CAST(GPUSamplerVk, GPUSampler)

 private:
  VkFilter ConvertFilter(GPUFilterMode filter) const;
  VkSamplerMipmapMode ConvertMipmapMode(GPUMipmapMode mipmap_mode) const;
  VkSamplerAddressMode ConvertAddressMode(GPUAddressMode address_mode) const;

  VkSampler sampler_ = VK_NULL_HANDLE;
  GPUDeviceVk* device_ = nullptr;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_SAMPLER_VK_HPP
