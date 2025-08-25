// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_render_pipeline_vk.hpp"

#include "src/gpu/vk/gpu_descriptor_set_vk.hpp"
#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/gpu_shader_function_vk.hpp"
#include "src/logging.hpp"

namespace skity {

GPURenderPipelineVk::GPURenderPipelineVk(
    GPUDeviceVk* device, const GPURenderPipelineDescriptor& desc)
    : GPURenderPipeline(desc), device_(device) {
  descriptor_manager_ = std::make_unique<GPUDescriptorManagerVk>(device);
}

GPURenderPipelineVk::~GPURenderPipelineVk() {
  VkDevice vk_device = device_->GetDevice();

  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(vk_device, pipeline_, nullptr);
  }

  if (pipeline_layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(vk_device, pipeline_layout_, nullptr);
  }

  // Clean up descriptor set layouts
  for (auto layout : descriptor_set_layouts_) {
    if (layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(vk_device, layout, nullptr);
    }
  }
}

bool GPURenderPipelineVk::IsValid() const {
  return valid_ && pipeline_ != VK_NULL_HANDLE &&
         pipeline_layout_ != VK_NULL_HANDLE;
}

std::unique_ptr<GPURenderPipelineVk> GPURenderPipelineVk::Create(
    GPUDeviceVk* device, const GPURenderPipelineDescriptor& desc) {
  auto pipeline = std::make_unique<GPURenderPipelineVk>(device, desc);

  if (!pipeline->CreatePipelineLayout()) {
    LOGE("Failed to create pipeline layout");
    return nullptr;
  }

  if (!pipeline->CreateGraphicsPipeline()) {
    LOGE("Failed to create graphics pipeline");
    return nullptr;
  }

  pipeline->valid_ = true;
  LOGI("Successfully created Vulkan render pipeline");
  return pipeline;
}

bool GPURenderPipelineVk::CreatePipelineLayout() {
  // Extract descriptor bindings from both vertex and fragment shader bind
  // groups
  std::vector<DescriptorBinding> shader_bindings;

  // TODO: use reflection
  for (uint32_t binding_idx = 0; binding_idx <= 3; ++binding_idx) {
    DescriptorBinding binding;
    binding.binding = binding_idx;
    binding.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.count = 1;
    binding.stage_flags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_bindings.push_back(binding);
  }

  // Store the corrected shader bindings for later use in descriptor set
  // creation
  shader_bindings_ = shader_bindings;

  // Create descriptor set layout manually instead of using the manager to avoid
  // recursion
  std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
  layout_bindings.reserve(shader_bindings.size());

  for (const auto& binding : shader_bindings) {
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = binding.binding;
    layout_binding.descriptorType = binding.type;
    layout_binding.descriptorCount = binding.count;
    layout_binding.stageFlags = binding.stage_flags;
    layout_binding.pImmutableSamplers = nullptr;

    layout_bindings.push_back(layout_binding);
  }

  VkDescriptorSetLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
  layout_info.pBindings = layout_bindings.data();

  VkDevice vk_device = device_->GetDevice();
  VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
  VkResult layout_result = vkCreateDescriptorSetLayout(
      vk_device, &layout_info, nullptr, &descriptor_layout);
  if (layout_result == VK_SUCCESS && descriptor_layout != VK_NULL_HANDLE) {
    descriptor_set_layouts_.push_back(descriptor_layout);
    LOGI("Created descriptor set layout with %zu bindings",
         shader_bindings.size());
  } else {
    LOGW("Failed to create descriptor set layout: %d, proceeding without it",
         layout_result);
  }

  VkPipelineLayoutCreateInfo pipeline_layout_info = {};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount =
      static_cast<uint32_t>(descriptor_set_layouts_.size());
  pipeline_layout_info.pSetLayouts = descriptor_set_layouts_.data();
  pipeline_layout_info.pushConstantRangeCount = 0;
  pipeline_layout_info.pPushConstantRanges = nullptr;

  VkResult result = vkCreatePipelineLayout(vk_device, &pipeline_layout_info,
                                           nullptr, &pipeline_layout_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create pipeline layout: %d", result);
    return false;
  }

  LOGI("Successfully created pipeline layout with %zu descriptor sets",
       descriptor_set_layouts_.size());
  return true;
}

bool GPURenderPipelineVk::CreateGraphicsPipeline() {
  const auto& desc = GetDescriptor();

  // Get shader modules from shader functions
  auto* vertex_shader_vk =
      static_cast<GPUShaderFunctionVk*>(desc.vertex_function.get());
  auto* fragment_shader_vk =
      static_cast<GPUShaderFunctionVk*>(desc.fragment_function.get());

  if (!vertex_shader_vk || !fragment_shader_vk) {
    LOGE("Invalid shader functions provided");
    return false;
  }

  // Shader stages
  std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

  VkPipelineShaderStageCreateInfo vertex_stage_info = {};
  vertex_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertex_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_stage_info.module = vertex_shader_vk->GetShaderModule();
  vertex_stage_info.pName = "main";
  shader_stages.push_back(vertex_stage_info);

  VkPipelineShaderStageCreateInfo fragment_stage_info = {};
  fragment_stage_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragment_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragment_stage_info.module = fragment_shader_vk->GetShaderModule();
  fragment_stage_info.pName = "main";
  shader_stages.push_back(fragment_stage_info);

  // Vertex input state
  VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  // Convert vertex buffer layouts to Vulkan format
  std::vector<VkVertexInputBindingDescription> binding_descriptions;
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

  const auto& vertex_buffers = desc.buffers;
  binding_descriptions.reserve(vertex_buffers.size());

  for (uint32_t binding = 0; binding < vertex_buffers.size(); ++binding) {
    const auto& buffer_layout = vertex_buffers[binding];

    VkVertexInputBindingDescription binding_desc = {};
    binding_desc.binding = binding;
    binding_desc.stride = static_cast<uint32_t>(buffer_layout.array_stride);
    binding_desc.inputRate =
        (buffer_layout.step_mode == GPUVertexStepMode::kInstance)
            ? VK_VERTEX_INPUT_RATE_INSTANCE
            : VK_VERTEX_INPUT_RATE_VERTEX;

    binding_descriptions.push_back(binding_desc);

    // Add attributes for this binding
    for (const auto& attribute : buffer_layout.attributes) {
      VkVertexInputAttributeDescription attr_desc = {};
      attr_desc.binding = binding;
      attr_desc.location = static_cast<uint32_t>(attribute.shader_location);
      attr_desc.format = ConvertVertexFormat(attribute.format);
      attr_desc.offset = static_cast<uint32_t>(attribute.offset);

      attribute_descriptions.push_back(attr_desc);
    }
  }

  vertex_input_info.vertexBindingDescriptionCount =
      static_cast<uint32_t>(binding_descriptions.size());
  vertex_input_info.pVertexBindingDescriptions = binding_descriptions.data();
  vertex_input_info.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attribute_descriptions.size());
  vertex_input_info.pVertexAttributeDescriptions =
      attribute_descriptions.data();

  LOGI("Pipeline vertex input: %u bindings, %u attributes",
       static_cast<uint32_t>(binding_descriptions.size()),
       static_cast<uint32_t>(attribute_descriptions.size()));

  // Input assembly state
  VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  // Viewport state
  VkPipelineViewportStateCreateInfo viewport_state = {};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = nullptr;  // Dynamic
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = nullptr;  // Dynamic

  // Rasterization state
  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;  // FIXED: Disable culling to ensure
                                            // triangles are visible
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  // Multisampling state
  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // Depth stencil state
  VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

  // Follow the same depth testing logic as GL backend
  bool enable_depth = desc.depth_stencil.enable_depth;
  depth_stencil.depthTestEnable = enable_depth ? VK_TRUE : VK_FALSE;
  depth_stencil.depthWriteEnable =
      (enable_depth && desc.depth_stencil.depth_state.enableWrite) ? VK_TRUE
                                                                   : VK_FALSE;
  depth_stencil.depthCompareOp =
      ConvertCompareFunction(desc.depth_stencil.depth_state.compare);

  depth_stencil.depthBoundsTestEnable = VK_FALSE;

  // Enable stencil testing to match GL backend behavior
  bool enable_stencil = desc.depth_stencil.enable_stencil;
  depth_stencil.stencilTestEnable = enable_stencil ? VK_TRUE : VK_FALSE;
  has_stencil_testing_ = enable_stencil;  // Remember for later use

  if (enable_stencil) {
    // Set up stencil state from pipeline descriptor
    const auto& stencil_state = desc.depth_stencil.stencil_state;

    // Front face stencil
    depth_stencil.front.failOp =
        ConvertStencilOperation(stencil_state.front.fail_op);
    depth_stencil.front.passOp =
        ConvertStencilOperation(stencil_state.front.pass_op);
    depth_stencil.front.depthFailOp =
        ConvertStencilOperation(stencil_state.front.depth_fail_op);
    depth_stencil.front.compareOp =
        ConvertCompareFunction(stencil_state.front.compare);
    depth_stencil.front.compareMask = stencil_state.front.stencil_read_mask;
    depth_stencil.front.writeMask = stencil_state.front.stencil_write_mask;
    depth_stencil.front.reference = 0;  // Will be set dynamically

    // Back face stencil
    depth_stencil.back.failOp =
        ConvertStencilOperation(stencil_state.back.fail_op);
    depth_stencil.back.passOp =
        ConvertStencilOperation(stencil_state.back.pass_op);
    depth_stencil.back.depthFailOp =
        ConvertStencilOperation(stencil_state.back.depth_fail_op);
    depth_stencil.back.compareOp =
        ConvertCompareFunction(stencil_state.back.compare);
    depth_stencil.back.compareMask = stencil_state.back.stencil_read_mask;
    depth_stencil.back.writeMask = stencil_state.back.stencil_write_mask;
    depth_stencil.back.reference = 0;  // Will be set dynamically
  }

  // Color blending - should match the blend factors from the pipeline
  // descriptor
  VkPipelineColorBlendAttachmentState color_blend_attachment = {};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_TRUE;

  // Use the blend factors from the pipeline descriptor
  const auto& target = desc.target;
  color_blend_attachment.srcColorBlendFactor =
      ConvertBlendFactor(target.src_blend_factor);
  color_blend_attachment.dstColorBlendFactor =
      ConvertBlendFactor(target.dst_blend_factor);
  color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor =
      ConvertBlendFactor(target.src_blend_factor);
  color_blend_attachment.dstAlphaBlendFactor =
      ConvertBlendFactor(target.dst_blend_factor);
  color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo color_blending = {};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  // Dynamic state
  std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT,
                                                VK_DYNAMIC_STATE_SCISSOR};

  // Add dynamic stencil reference if stencil testing is enabled
  if (enable_stencil) {
    dynamic_states.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
  }

  VkPipelineDynamicStateCreateInfo dynamic_state = {};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount =
      static_cast<uint32_t>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  // Create graphics pipeline
  VkGraphicsPipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
  pipeline_info.pStages = shader_stages.data();
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout_;
  pipeline_info.renderPass = device_->GetCompatibleRenderPass(
      VK_FORMAT_B8G8R8A8_SRGB,
      true);  // Always use depth/stencil render pass for compatibility
  pipeline_info.subpass = 0;
  pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
  pipeline_info.basePipelineIndex = -1;

  VkDevice vk_device = device_->GetDevice();
  VkResult result = vkCreateGraphicsPipelines(
      vk_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create graphics pipeline: %d", result);
    // Note: This may fail due to placeholder SPIRV shaders, but the pipeline
    // system is implemented
    return false;
  }

  LOGI("Successfully created Vulkan graphics pipeline");
  return true;
}

// Helper conversion functions
VkShaderStageFlags GPURenderPipelineVk::ConvertShaderStageFlags(
    GPUShaderStageMask stages) {
  VkShaderStageFlags vk_stages = 0;
  if (stages & static_cast<uint32_t>(GPUShaderStage::kVertex)) {
    vk_stages |= VK_SHADER_STAGE_VERTEX_BIT;
  }
  if (stages & static_cast<uint32_t>(GPUShaderStage::kFragment)) {
    vk_stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  return vk_stages;
}

VkFormat GPURenderPipelineVk::ConvertVertexFormat(GPUVertexFormat format) {
  switch (format) {
    case GPUVertexFormat::kFloat32:
      return VK_FORMAT_R32_SFLOAT;
    case GPUVertexFormat::kFloat32x2:
      return VK_FORMAT_R32G32_SFLOAT;
    case GPUVertexFormat::kFloat32x3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case GPUVertexFormat::kFloat32x4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

VkCompareOp GPURenderPipelineVk::ConvertCompareFunction(
    GPUCompareFunction func) {
  switch (func) {
    case GPUCompareFunction::kNever:
      return VK_COMPARE_OP_NEVER;
    case GPUCompareFunction::kLess:
      return VK_COMPARE_OP_LESS;
    case GPUCompareFunction::kEqual:
      return VK_COMPARE_OP_EQUAL;
    case GPUCompareFunction::kLessEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case GPUCompareFunction::kGreater:
      return VK_COMPARE_OP_GREATER;
    case GPUCompareFunction::kNotEqual:
      return VK_COMPARE_OP_NOT_EQUAL;
    case GPUCompareFunction::kGreaterEqual:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case GPUCompareFunction::kAlways:
      return VK_COMPARE_OP_ALWAYS;
    default:
      return VK_COMPARE_OP_ALWAYS;
  }
}

VkStencilOp GPURenderPipelineVk::ConvertStencilOperation(
    GPUStencilOperation op) {
  switch (op) {
    case GPUStencilOperation::kKeep:
      return VK_STENCIL_OP_KEEP;
    case GPUStencilOperation::kZero:
      return VK_STENCIL_OP_ZERO;
    case GPUStencilOperation::kReplace:
      return VK_STENCIL_OP_REPLACE;
    case GPUStencilOperation::kInvert:
      return VK_STENCIL_OP_INVERT;
    case GPUStencilOperation::kIncrementClamp:
      return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case GPUStencilOperation::kDecrementClamp:
      return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case GPUStencilOperation::kIncrementWrap:
      return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case GPUStencilOperation::kDecrementWrap:
      return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    default:
      return VK_STENCIL_OP_KEEP;
  }
}

VkBlendFactor GPURenderPipelineVk::ConvertBlendFactor(GPUBlendFactor factor) {
  switch (factor) {
    case GPUBlendFactor::kZero:
      return VK_BLEND_FACTOR_ZERO;
    case GPUBlendFactor::kOne:
      return VK_BLEND_FACTOR_ONE;
    case GPUBlendFactor::kSrc:
      return VK_BLEND_FACTOR_SRC_COLOR;
    case GPUBlendFactor::kOneMinusSrc:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case GPUBlendFactor::kSrcAlpha:
      return VK_BLEND_FACTOR_SRC_ALPHA;
    case GPUBlendFactor::kOneMinusSrcAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case GPUBlendFactor::kDst:
      return VK_BLEND_FACTOR_DST_COLOR;
    case GPUBlendFactor::kOneMinusDst:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case GPUBlendFactor::kDstAlpha:
      return VK_BLEND_FACTOR_DST_ALPHA;
    case GPUBlendFactor::kOneMinusDstAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case GPUBlendFactor::kSrcAlphaSaturated:
      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    default:
      return VK_BLEND_FACTOR_ONE;
  }
}

std::shared_ptr<GPUDescriptorSetVk> GPURenderPipelineVk::CreateDescriptorSet(
    const std::vector<DescriptorBinding>& bindings) {
  return descriptor_manager_->CreateDescriptorSet(bindings);
}

void GPURenderPipelineVk::BindDescriptorSet(
    VkCommandBuffer command_buffer,
    std::shared_ptr<GPUDescriptorSetVk> descriptor_set) {
  if (!descriptor_set) {
    LOGE("Invalid descriptor set for binding");
    return;
  }

  VkDescriptorSet vk_descriptor_set = descriptor_set->GetDescriptorSet();
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout_, 0, 1, &vk_descriptor_set, 0,
                          nullptr);

  LOGI("Bound descriptor set to pipeline");
}

std::shared_ptr<GPUDescriptorSetVk>
GPURenderPipelineVk::CreateDescriptorSetUsingPipelineLayout() {
  // Use the stored shader bindings from reflection to create a descriptor set
  // that matches the pipeline's layout
  return descriptor_manager_->CreateDescriptorSet(shader_bindings_);
}

}  // namespace skity