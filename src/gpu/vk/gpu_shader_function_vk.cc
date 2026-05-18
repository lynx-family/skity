// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_shader_function_vk.hpp"

#include "src/gpu/vk/vulkan_context_state.hpp"

namespace skity {

GPUShaderFunctionVK::GPUShaderFunctionVK(
    GPULabel label, GPUShaderStage stage, std::string entry_point,
    std::shared_ptr<const VulkanContextState> state,
    VkShaderModule shader_module)
    : GPUShaderFunction(std::move(label)),
      stage_(stage),
      entry_point_(std::move(entry_point)),
      state_(std::move(state)),
      shader_module_(shader_module) {}

GPUShaderFunctionVK::~GPUShaderFunctionVK() {
  if (shader_module_ != VK_NULL_HANDLE && state_ != nullptr &&
      state_->DeviceFns().vkDestroyShaderModule != nullptr &&
      state_->GetLogicalDevice() != VK_NULL_HANDLE) {
    state_->DeviceFns().vkDestroyShaderModule(state_->GetLogicalDevice(),
                                              shader_module_, nullptr);
  }
}

}  // namespace skity
