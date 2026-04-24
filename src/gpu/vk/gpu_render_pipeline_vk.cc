// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_render_pipeline_vk.hpp"

#include "src/gpu/vk/vulkan_context_state.hpp"

namespace skity {

GPURenderPipelineVK::GPURenderPipelineVK(
    std::shared_ptr<const VulkanContextState> state, VkPipeline pipeline,
    VkPipelineLayout pipeline_layout, VkRenderPass render_pass,
    bool uses_dynamic_rendering,
    std::vector<VkDescriptorSetLayout> set_layouts,
    const GPURenderPipelineDescriptor& desc)
    : GPURenderPipeline(desc),
      state_(std::move(state)),
      pipeline_(pipeline),
      pipeline_layout_(pipeline_layout),
      render_pass_(render_pass),
      uses_dynamic_rendering_(uses_dynamic_rendering),
      set_layouts_(std::move(set_layouts)) {}

GPURenderPipelineVK::~GPURenderPipelineVK() {
  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE) {
    return;
  }

  const auto& device_fns = state_->DeviceFns();
  if (pipeline_ != VK_NULL_HANDLE && device_fns.vkDestroyPipeline != nullptr) {
    device_fns.vkDestroyPipeline(state_->GetLogicalDevice(), pipeline_, nullptr);
  }

  if (pipeline_layout_ != VK_NULL_HANDLE &&
      device_fns.vkDestroyPipelineLayout != nullptr) {
    device_fns.vkDestroyPipelineLayout(state_->GetLogicalDevice(),
                                      pipeline_layout_, nullptr);
  }

  if (device_fns.vkDestroyDescriptorSetLayout != nullptr) {
    for (auto set_layout : set_layouts_) {
      if (set_layout != VK_NULL_HANDLE) {
        device_fns.vkDestroyDescriptorSetLayout(state_->GetLogicalDevice(),
                                                set_layout, nullptr);
      }
    }
  }
}

}  // namespace skity
