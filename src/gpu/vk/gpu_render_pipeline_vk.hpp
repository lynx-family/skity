// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_RENDER_PIPELINE_VK_HPP
#define SRC_GPU_VK_GPU_RENDER_PIPELINE_VK_HPP

#include <vulkan/vulkan.h>

#include "src/gpu/backend_cast.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"

namespace skity {

class VulkanContextState;

class GPURenderPipelineVK : public GPURenderPipeline {
 public:
  GPURenderPipelineVK(std::shared_ptr<const VulkanContextState> state,
                      VkPipeline pipeline, VkPipelineLayout pipeline_layout,
                      VkRenderPass render_pass, bool uses_dynamic_rendering,
                      std::vector<VkDescriptorSetLayout> set_layouts,
                      const GPURenderPipelineDescriptor& desc);

  ~GPURenderPipelineVK() override;

  VkPipeline GetPipeline() const { return pipeline_; }

  VkPipelineLayout GetPipelineLayout() const { return pipeline_layout_; }

  VkRenderPass GetRenderPass() const { return render_pass_; }

  bool UsesDynamicRendering() const { return uses_dynamic_rendering_; }

  const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() const {
    return set_layouts_;
  }

  bool IsValid() const override {
    return GPURenderPipeline::IsValid() && pipeline_ != VK_NULL_HANDLE &&
           pipeline_layout_ != VK_NULL_HANDLE &&
           (uses_dynamic_rendering_ || render_pass_ != VK_NULL_HANDLE);
  }

  SKT_BACKEND_CAST(GPURenderPipelineVK, GPURenderPipeline)

 private:
  std::shared_ptr<const VulkanContextState> state_ = {};
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  bool uses_dynamic_rendering_ = false;
  std::vector<VkDescriptorSetLayout> set_layouts_ = {};
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_RENDER_PIPELINE_VK_HPP
