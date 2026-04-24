// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_SAMPLER_VK_HPP
#define SRC_GPU_VK_GPU_SAMPLER_VK_HPP

#include <vulkan/vulkan.h>

#include "src/gpu/backend_cast.hpp"
#include "src/gpu/gpu_sampler.hpp"

namespace skity {

class VulkanContextState;

class GPUSamplerVK : public GPUSampler {
 public:
  GPUSamplerVK(std::shared_ptr<const VulkanContextState> state,
               const GPUSamplerDescriptor& desc, VkSampler sampler);

  ~GPUSamplerVK() override;

  VkSampler GetSampler() const { return sampler_; }

  bool IsValid() const { return sampler_ != VK_NULL_HANDLE; }

  SKT_BACKEND_CAST(GPUSamplerVK, GPUSampler)

 private:
  std::shared_ptr<const VulkanContextState> state_ = {};
  VkSampler sampler_ = VK_NULL_HANDLE;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_SAMPLER_VK_HPP
