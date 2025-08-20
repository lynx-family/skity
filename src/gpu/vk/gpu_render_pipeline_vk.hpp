// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_RENDER_PIPELINE_VK_HPP
#define SRC_GPU_VK_GPU_RENDER_PIPELINE_VK_HPP

#include <volk.h>

#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/gpu/vk/gpu_descriptor_set_vk.hpp"

namespace skity {

class GPUDeviceVk;

class GPURenderPipelineVk : public GPURenderPipeline {
 public:
  GPURenderPipelineVk(GPUDeviceVk* device,
                      const GPURenderPipelineDescriptor& desc);
  ~GPURenderPipelineVk() override;

  bool IsValid() const override;

  static std::unique_ptr<GPURenderPipelineVk> Create(
      GPUDeviceVk* device, const GPURenderPipelineDescriptor& desc);

  VkPipeline GetVkPipeline() const { return pipeline_; }
  VkPipelineLayout GetVkPipelineLayout() const { return pipeline_layout_; }

  // Descriptor set management
  std::shared_ptr<GPUDescriptorSetVk> CreateDescriptorSet(
      const std::vector<DescriptorBinding>& bindings);
  void BindDescriptorSet(VkCommandBuffer command_buffer,
                         std::shared_ptr<GPUDescriptorSetVk> descriptor_set);

  // Create descriptor set using the pipeline's own descriptor layout from
  // shader reflection
  std::shared_ptr<GPUDescriptorSetVk> CreateDescriptorSetUsingPipelineLayout();

  GPUDescriptorManagerVk* GetDescriptorManager() {
    return descriptor_manager_.get();
  }

  // Check if this pipeline has stencil testing enabled
  bool HasStencilTesting() const { return has_stencil_testing_; }

 private:
  bool CreatePipelineLayout();
  bool CreateGraphicsPipeline();

  VkShaderStageFlags ConvertShaderStageFlags(GPUShaderStageMask stages);
  VkFormat ConvertVertexFormat(GPUVertexFormat format);
  VkCompareOp ConvertCompareFunction(GPUCompareFunction func);
  VkStencilOp ConvertStencilOperation(GPUStencilOperation op);
  VkBlendFactor ConvertBlendFactor(GPUBlendFactor factor);

  GPUDeviceVk* device_ = nullptr;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  std::unique_ptr<GPUDescriptorManagerVk> descriptor_manager_;
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts_;
  std::vector<DescriptorBinding>
      shader_bindings_;  // Store bindings from shader reflection
  bool has_stencil_testing_ =
      false;  // Track if this pipeline uses stencil testing
  bool valid_ = false;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_RENDER_PIPELINE_VK_HPP