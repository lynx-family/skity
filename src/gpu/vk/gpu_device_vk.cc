// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_device_vk.hpp"

#include <algorithm>
#include <array>

#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/gpu_render_pipeline_vk.hpp"
#include "src/gpu/vk/gpu_sampler_vk.hpp"
#include "src/gpu/vk/gpu_shader_function_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"
#include "src/tracing.hpp"

namespace skity {

namespace {

VkShaderStageFlagBits ToVkShaderStage(skity::GPUShaderStage stage) {
  switch (stage) {
    case skity::GPUShaderStage::kVertex:
      return VK_SHADER_STAGE_VERTEX_BIT;
    case skity::GPUShaderStage::kFragment:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
  }

  return VK_SHADER_STAGE_VERTEX_BIT;
}

VkShaderStageFlags ToVkShaderStageFlags(wgx::ShaderStage stage) {
  VkShaderStageFlags flags = 0;
  if ((stage & wgx::ShaderStage_kVertex) != 0) {
    flags |= VK_SHADER_STAGE_VERTEX_BIT;
  }
  if ((stage & wgx::ShaderStage_kFragment) != 0) {
    flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  return flags;
}

VkDescriptorType ToVkDescriptorType(wgx::BindingType type) {
  switch (type) {
    case wgx::BindingType::kUniformBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case wgx::BindingType::kTexture:
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case wgx::BindingType::kSampler:
      return VK_DESCRIPTOR_TYPE_SAMPLER;
    case wgx::BindingType::kUndefined:
      break;
  }

  return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

VkBlendFactor ToVkBlendFactor(skity::GPUBlendFactor factor) {
  switch (factor) {
    case skity::GPUBlendFactor::kZero:
      return VK_BLEND_FACTOR_ZERO;
    case skity::GPUBlendFactor::kOne:
      return VK_BLEND_FACTOR_ONE;
    case skity::GPUBlendFactor::kSrc:
      return VK_BLEND_FACTOR_SRC_COLOR;
    case skity::GPUBlendFactor::kOneMinusSrc:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case skity::GPUBlendFactor::kSrcAlpha:
      return VK_BLEND_FACTOR_SRC_ALPHA;
    case skity::GPUBlendFactor::kOneMinusSrcAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case skity::GPUBlendFactor::kDst:
      return VK_BLEND_FACTOR_DST_COLOR;
    case skity::GPUBlendFactor::kOneMinusDst:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case skity::GPUBlendFactor::kDstAlpha:
      return VK_BLEND_FACTOR_DST_ALPHA;
    case skity::GPUBlendFactor::kOneMinusDstAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case skity::GPUBlendFactor::kSrcAlphaSaturated:
      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  }

  return VK_BLEND_FACTOR_ONE;
}

VkCompareOp ToVkCompareOp(skity::GPUCompareFunction compare) {
  switch (compare) {
    case skity::GPUCompareFunction::kNever:
      return VK_COMPARE_OP_NEVER;
    case skity::GPUCompareFunction::kLess:
      return VK_COMPARE_OP_LESS;
    case skity::GPUCompareFunction::kEqual:
      return VK_COMPARE_OP_EQUAL;
    case skity::GPUCompareFunction::kLessEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case skity::GPUCompareFunction::kGreater:
      return VK_COMPARE_OP_GREATER;
    case skity::GPUCompareFunction::kNotEqual:
      return VK_COMPARE_OP_NOT_EQUAL;
    case skity::GPUCompareFunction::kGreaterEqual:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case skity::GPUCompareFunction::kAlways:
      return VK_COMPARE_OP_ALWAYS;
  }

  return VK_COMPARE_OP_ALWAYS;
}

VkStencilOp ToVkStencilOp(skity::GPUStencilOperation op) {
  switch (op) {
    case skity::GPUStencilOperation::kKeep:
      return VK_STENCIL_OP_KEEP;
    case skity::GPUStencilOperation::kZero:
      return VK_STENCIL_OP_ZERO;
    case skity::GPUStencilOperation::kReplace:
      return VK_STENCIL_OP_REPLACE;
    case skity::GPUStencilOperation::kInvert:
      return VK_STENCIL_OP_INVERT;
    case skity::GPUStencilOperation::kIncrementClamp:
      return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case skity::GPUStencilOperation::kDecrementClamp:
      return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case skity::GPUStencilOperation::kIncrementWrap:
      return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case skity::GPUStencilOperation::kDecrementWrap:
      return VK_STENCIL_OP_DECREMENT_AND_WRAP;
  }

  return VK_STENCIL_OP_KEEP;
}

VkStencilOpState ToVkStencilOpState(const skity::GPUStencilFaceState& state) {
  VkStencilOpState vk_state = {};
  vk_state.failOp = ToVkStencilOp(state.fail_op);
  vk_state.passOp = ToVkStencilOp(state.pass_op);
  vk_state.depthFailOp = ToVkStencilOp(state.depth_fail_op);
  vk_state.compareOp = ToVkCompareOp(state.compare);
  vk_state.compareMask = 0xFFFFFFFFu;
  vk_state.writeMask = 0xFFFFFFFFu;
  return vk_state;
}

VkVertexInputRate ToVkVertexInputRate(skity::GPUVertexStepMode step_mode) {
  return step_mode == skity::GPUVertexStepMode::kInstance
             ? VK_VERTEX_INPUT_RATE_INSTANCE
             : VK_VERTEX_INPUT_RATE_VERTEX;
}

VkFormat ToVkVertexFormat(skity::GPUVertexFormat format) {
  switch (format) {
    case skity::GPUVertexFormat::kFloat32:
      return VK_FORMAT_R32_SFLOAT;
    case skity::GPUVertexFormat::kFloat32x2:
      return VK_FORMAT_R32G32_SFLOAT;
    case skity::GPUVertexFormat::kFloat32x3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case skity::GPUVertexFormat::kFloat32x4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
  }

  return VK_FORMAT_UNDEFINED;
}

VkColorComponentFlags ToVkColorWriteMask(int32_t write_mask) {
  VkColorComponentFlags result = 0;
  if ((write_mask & 0x1) != 0) {
    result |= VK_COLOR_COMPONENT_R_BIT;
  }
  if ((write_mask & 0x2) != 0) {
    result |= VK_COLOR_COMPONENT_G_BIT;
  }
  if ((write_mask & 0x4) != 0) {
    result |= VK_COLOR_COMPONENT_B_BIT;
  }
  if ((write_mask & 0x8) != 0) {
    result |= VK_COLOR_COMPONENT_A_BIT;
  }
  return result;
}

VkSamplerAddressMode ToVkSamplerAddressMode(skity::GPUAddressMode mode) {
  switch (mode) {
    case skity::GPUAddressMode::kRepeat:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case skity::GPUAddressMode::kMirrorRepeat:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case skity::GPUAddressMode::kClampToEdge:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }

  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

VkFilter ToVkFilter(skity::GPUFilterMode mode) {
  switch (mode) {
    case skity::GPUFilterMode::kLinear:
      return VK_FILTER_LINEAR;
    case skity::GPUFilterMode::kNearest:
      return VK_FILTER_NEAREST;
  }

  return VK_FILTER_NEAREST;
}

bool HasDepthAttachmentFormat(skity::GPUTextureFormat format) {
  return format == skity::GPUTextureFormat::kDepth24Stencil8;
}

bool HasStencilAttachmentFormat(skity::GPUTextureFormat format) {
  return format == skity::GPUTextureFormat::kStencil8 ||
         format == skity::GPUTextureFormat::kDepth24Stencil8;
}

bool HasDepthStencilAttachmentFormat(skity::GPUTextureFormat format) {
  return HasDepthAttachmentFormat(format) || HasStencilAttachmentFormat(format);
}

bool IsAttachmentFormatSupported(const skity::VulkanContextState& state,
                                 VkFormat format, VkImageUsageFlags usage,
                                 VkSampleCountFlagBits samples) {
  if (format == VK_FORMAT_UNDEFINED ||
      state.InstanceFns().vkGetPhysicalDeviceImageFormatProperties == nullptr) {
    return false;
  }

  VkImageFormatProperties properties = {};
  const VkResult result =
      state.InstanceFns().vkGetPhysicalDeviceImageFormatProperties(
          state.GetPhysicalDevice(), format, VK_IMAGE_TYPE_2D,
          VK_IMAGE_TILING_OPTIMAL, usage, 0, &properties);
  if (result != VK_SUCCESS) {
    return false;
  }

  return (properties.sampleCounts & samples) != 0;
}

VkFormat ResolveAttachmentFormat(const skity::VulkanContextState& state,
                                 skity::GPUTextureFormat format,
                                 VkSampleCountFlagBits samples) {
  const VkImageUsageFlags usage =
      HasDepthStencilAttachmentFormat(format)
          ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
          : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  const VkFormat preferred = skity::GPUTextureVK::ToVkFormat(format);
  if (IsAttachmentFormatSupported(state, preferred, usage, samples)) {
    return preferred;
  }

  if (format == skity::GPUTextureFormat::kDepth24Stencil8 &&
      IsAttachmentFormatSupported(state, VK_FORMAT_D32_SFLOAT_S8_UINT, usage,
                                  samples)) {
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
  }

  return VK_FORMAT_UNDEFINED;
}

VkSamplerMipmapMode ToVkSamplerMipmapMode(skity::GPUMipmapMode mode) {
  switch (mode) {
    case skity::GPUMipmapMode::kLinear:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    case skity::GPUMipmapMode::kNearest:
    case skity::GPUMipmapMode::kNone:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }

  return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

bool CreateDescriptorSetLayout(const skity::VulkanContextState& state,
                               const wgx::BindGroup& bind_group,
                               VkDescriptorSetLayout* set_layout) {
  if (set_layout == nullptr) {
    return false;
  }

  std::vector<VkDescriptorSetLayoutBinding> bindings;
  bindings.reserve(bind_group.entries.size());
  for (const auto& entry : bind_group.entries) {
    const VkDescriptorType descriptor_type = ToVkDescriptorType(entry.type);
    if (descriptor_type == VK_DESCRIPTOR_TYPE_MAX_ENUM) {
      LOGE("Unsupported Vulkan bind group entry type: {}",
           static_cast<uint32_t>(entry.type));
      return false;
    }

    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = entry.binding;
    binding.descriptorType = descriptor_type;
    binding.descriptorCount = 1;
    binding.stageFlags = ToVkShaderStageFlags(entry.stage);
    bindings.push_back(binding);
  }

  VkDescriptorSetLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
  layout_info.pBindings = bindings.empty() ? nullptr : bindings.data();

  *set_layout = VK_NULL_HANDLE;
  const VkResult result = state.DeviceFns().vkCreateDescriptorSetLayout(
      state.GetLogicalDevice(), &layout_info, nullptr, set_layout);
  if (result != VK_SUCCESS || *set_layout == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan descriptor set layout: {}",
         static_cast<int32_t>(result));
    *set_layout = VK_NULL_HANDLE;
    return false;
  }

  return true;
}

skity::VulkanContextState::LegacyRenderPassKey BuildPipelineRenderPassKey(
    const skity::VulkanContextState& state,
    const skity::GPURenderPipelineDescriptor& desc) {
  skity::VulkanContextState::LegacyRenderPassKey key = {};
  key.color_samples = static_cast<VkSampleCountFlagBits>(desc.sample_count);
  key.color_format =
      ResolveAttachmentFormat(state, desc.target.format, key.color_samples);
  key.color_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  key.color_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  key.has_depth = HasDepthAttachmentFormat(desc.depth_stencil.format);
  key.has_stencil = HasStencilAttachmentFormat(desc.depth_stencil.format);
  if (key.has_depth || key.has_stencil) {
    key.depth_stencil_format = ResolveAttachmentFormat(
        state, desc.depth_stencil.format, key.color_samples);
    key.depth_stencil_samples = key.color_samples;
    key.depth_load_op = key.has_depth ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                                      : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    key.depth_store_op = key.has_depth ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                       : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    key.stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    key.stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }

  return key;
}

struct PipelineTargetInfo {
  VkRenderPass render_pass = VK_NULL_HANDLE;
  bool uses_dynamic_rendering = false;
  VkPipelineRenderingCreateInfo rendering_info = {};
  VkFormat color_format = VK_FORMAT_UNDEFINED;
};

bool BuildPipelineTargetInfo(const skity::VulkanContextState& state,
                             const skity::GPURenderPipelineDescriptor& desc,
                             PipelineTargetInfo* target_info) {
  if (target_info == nullptr) {
    return false;
  }

  target_info->color_format = ResolveAttachmentFormat(
      state, desc.target.format,
      static_cast<VkSampleCountFlagBits>(desc.sample_count));
  if (target_info->color_format == VK_FORMAT_UNDEFINED) {
    LOGE("Unsupported Vulkan color attachment format for render pipeline");
    return false;
  }

  if (state.IsDynamicRenderingEnabled()) {
    const VkFormat depth_stencil_format =
        HasDepthStencilAttachmentFormat(desc.depth_stencil.format)
            ? ResolveAttachmentFormat(
                  state, desc.depth_stencil.format,
                  static_cast<VkSampleCountFlagBits>(desc.sample_count))
            : VK_FORMAT_UNDEFINED;
    if (HasDepthStencilAttachmentFormat(desc.depth_stencil.format) &&
        depth_stencil_format == VK_FORMAT_UNDEFINED) {
      LOGE(
          "Unsupported Vulkan depth/stencil attachment format for render "
          "pipeline");
      return false;
    }

    target_info->uses_dynamic_rendering = true;
    target_info->rendering_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    target_info->rendering_info.colorAttachmentCount = 1;
    target_info->rendering_info.pColorAttachmentFormats =
        &target_info->color_format;
    target_info->rendering_info.depthAttachmentFormat =
        HasDepthAttachmentFormat(desc.depth_stencil.format)
            ? depth_stencil_format
            : VK_FORMAT_UNDEFINED;
    target_info->rendering_info.stencilAttachmentFormat =
        HasStencilAttachmentFormat(desc.depth_stencil.format)
            ? depth_stencil_format
            : VK_FORMAT_UNDEFINED;
    return true;
  }

  const auto render_pass_key = BuildPipelineRenderPassKey(state, desc);
  target_info->render_pass = state.GetOrCreateLegacyRenderPass(render_pass_key);
  return target_info->render_pass != VK_NULL_HANDLE;
}

#if defined(SKITY_VK_DEBUG_RUNTIME)

void SetShaderModuleDebugLabel(const VulkanContextState& state,
                               VkShaderModule shader_module,
                               const std::string& label) {
  if (shader_module == VK_NULL_HANDLE || label.empty() ||
      !state.HasEnabledInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    return;
  }

  const auto& device_fns = state.DeviceFns();
  if (device_fns.vkSetDebugUtilsObjectNameEXT == nullptr ||
      state.GetLogicalDevice() == VK_NULL_HANDLE) {
    return;
  }

  VkDebugUtilsObjectNameInfoEXT name_info = {};
  name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  name_info.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
  name_info.objectHandle = reinterpret_cast<uint64_t>(shader_module);
  name_info.pObjectName = label.c_str();

  const VkResult result = device_fns.vkSetDebugUtilsObjectNameEXT(
      state.GetLogicalDevice(), &name_info);
  if (result != VK_SUCCESS) {
    LOGW("Failed to set Vulkan shader module debug label '{}': {}", label,
         static_cast<int32_t>(result));
  }
}

#else

void SetShaderModuleDebugLabel(const VulkanContextState& state,
                               VkShaderModule shader_module,
                               const std::string& label) {
  (void)state;
  (void)shader_module;
  (void)label;
}

#endif

}  // namespace

GPUDeviceVK::GPUDeviceVK(std::shared_ptr<const VulkanContextState> state)
    : state_(std::move(state)) {
  auto gpu_caps = std::make_unique<GPUCaps>();
  gpu_caps->supports_framebuffer_fetch = false;
  InitCaps(std::move(gpu_caps));

  if (state_ == nullptr || state_->GetPhysicalDevice() == VK_NULL_HANDLE ||
      state_->InstanceFns().vkGetPhysicalDeviceProperties == nullptr) {
    return;
  }

  VkPhysicalDeviceProperties properties = {};
  state_->InstanceFns().vkGetPhysicalDeviceProperties(
      state_->GetPhysicalDevice(), &properties);
  buffer_alignment_ =
      static_cast<uint32_t>(properties.limits.minUniformBufferOffsetAlignment);
  if (buffer_alignment_ == 0) {
    buffer_alignment_ = 256;
  }

  max_texture_size_ = properties.limits.maxImageDimension2D;
}

std::unique_ptr<GPUBuffer> GPUDeviceVK::CreateBuffer(GPUBufferUsageMask usage) {
  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE ||
      state_->GetAllocator() == nullptr) {
    LOGE("GPUDeviceVK::CreateBuffer failed: Vulkan device is unavailable");
    return {};
  }

  return std::make_unique<GPUBufferVK>(usage, state_);
}

std::shared_ptr<GPUShaderFunction> GPUDeviceVK::CreateShaderFunction(
    const GPUShaderFunctionDescriptor& desc) {
  if (desc.source_type != GPUShaderSourceType::kWGX) {
    LOGW("GPUDeviceVK only supports WGX shader sources currently");
    return {};
  }

  return CreateShaderFunctionFromModule(desc);
}

std::unique_ptr<GPURenderPipeline> GPUDeviceVK::CreateRenderPipeline(
    const GPURenderPipelineDescriptor& desc) {
  return CreateRenderPipelineInternal(desc);
}

std::unique_ptr<GPURenderPipeline> GPUDeviceVK::ClonePipeline(
    GPURenderPipeline* base, const GPURenderPipelineDescriptor& desc) {
  if (base == nullptr) {
    return {};
  }

  auto* vk_pipeline = GPURenderPipelineVK::Cast(base);
  if (vk_pipeline == nullptr || !vk_pipeline->IsValid()) {
    return {};
  }

  return CreateRenderPipelineInternal(desc);
}

std::shared_ptr<GPUCommandBuffer> GPUDeviceVK::CreateCommandBuffer() {
  auto command_buffer = std::make_shared<GPUCommandBufferVK>(state_);
  if (!command_buffer->Init()) {
    return {};
  }
  return command_buffer;
}

std::shared_ptr<GPUSampler> GPUDeviceVK::CreateSampler(
    const GPUSamplerDescriptor& desc) {
  auto it = sampler_map_.find(desc);
  if (it != sampler_map_.end()) {
    return it->second;
  }

  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE ||
      state_->DeviceFns().vkCreateSampler == nullptr) {
    LOGE("GPUDeviceVK::CreateSampler failed: Vulkan device is unavailable");
    return {};
  }

  VkSamplerCreateInfo sampler_info = {};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = ToVkFilter(desc.mag_filter);
  sampler_info.minFilter = ToVkFilter(desc.min_filter);
  sampler_info.mipmapMode = ToVkSamplerMipmapMode(desc.mipmap_filter);
  sampler_info.addressModeU = ToVkSamplerAddressMode(desc.address_mode_u);
  sampler_info.addressModeV = ToVkSamplerAddressMode(desc.address_mode_v);
  sampler_info.addressModeW = ToVkSamplerAddressMode(desc.address_mode_w);
  sampler_info.mipLodBias = 0.f;
  sampler_info.anisotropyEnable = VK_FALSE;
  sampler_info.maxAnisotropy = 1.f;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_info.minLod = 0.f;
  sampler_info.maxLod =
      desc.mipmap_filter == GPUMipmapMode::kNone ? 0.f : VK_LOD_CLAMP_NONE;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;

  VkSampler sampler = VK_NULL_HANDLE;
  const VkResult result = state_->DeviceFns().vkCreateSampler(
      state_->GetLogicalDevice(), &sampler_info, nullptr, &sampler);
  if (result != VK_SUCCESS || sampler == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan sampler: {}", static_cast<int32_t>(result));
    return {};
  }

  auto sampler_vk = std::make_shared<GPUSamplerVK>(state_, desc, sampler);
  sampler_map_.insert({desc, sampler_vk});
  return sampler_vk;
}

std::shared_ptr<GPUTexture> GPUDeviceVK::CreateTexture(
    const GPUTextureDescriptor& desc) {
  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE ||
      state_->GetAllocator() == nullptr) {
    LOGE("GPUDeviceVK::CreateTexture failed: Vulkan device is unavailable");
    return {};
  }

  return GPUTextureVK::Create(state_, desc);
}

std::shared_ptr<GPUShaderFunction> GPUDeviceVK::CreateShaderFunctionFromModule(
    const GPUShaderFunctionDescriptor& desc) {
  SKITY_TRACE_EVENT(GPUDeviceVK_CreateShaderFunctionFromModuleWGX);

  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE) {
    return {};
  }

  auto* source = reinterpret_cast<GPUShaderSourceWGX*>(desc.shader_source);
  if (source == nullptr || source->module == nullptr ||
      source->module->GetProgram() == nullptr ||
      source->entry_point == nullptr) {
    return {};
  }

  const auto& device_fns = state_->DeviceFns();
  if (device_fns.vkCreateShaderModule == nullptr ||
      device_fns.vkDestroyShaderModule == nullptr) {
    LOGE("Vulkan shader module procedures are not available");
    return {};
  }

  wgx::SpirvOptions options = {};
  auto wgx_result = source->module->GetProgram()->WriteToSpirv(
      source->entry_point, options, source->context);

  if (!wgx_result.success || wgx_result.spirv.empty()) {
    if (desc.error_callback) {
      desc.error_callback("WGX translate to SPIR-V failed");
    }
    return {};
  }

  VkShaderModuleCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = wgx_result.spirv.size() * sizeof(uint32_t);
  create_info.pCode = wgx_result.spirv.data();

  VkShaderModule shader_module = VK_NULL_HANDLE;
  VkResult result = device_fns.vkCreateShaderModule(
      state_->GetLogicalDevice(), &create_info, nullptr, &shader_module);
  if (result != VK_SUCCESS || shader_module == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan shader module for {}:{} result={}",
         source->module->GetLabel(), source->entry_point,
         static_cast<int32_t>(result));
    if (desc.error_callback) {
      desc.error_callback("Failed to create Vulkan shader module");
    }
    return {};
  }

  SetShaderModuleDebugLabel(*state_, shader_module, desc.label.ToString());

  auto function = std::make_shared<GPUShaderFunctionVK>(
      desc.label, desc.stage, source->entry_point, state_, shader_module);
  function->SetBindGroups(
      source->module->GetProgram()->GetWGSLBindGroups(source->entry_point));
  function->SetWGXContext(wgx_result.context);

  // pass the WGX context to caller for later pipeline compilation.
  source->context = wgx_result.context;

  return function;
}

std::unique_ptr<GPURenderPipelineVK> GPUDeviceVK::CreateRenderPipelineInternal(
    const GPURenderPipelineDescriptor& desc) {
  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE ||
      desc.vertex_function == nullptr || desc.fragment_function == nullptr) {
    return {};
  }

  auto* vertex_function = GPUShaderFunctionVK::Cast(desc.vertex_function.get());
  auto* fragment_function =
      GPUShaderFunctionVK::Cast(desc.fragment_function.get());
  if (vertex_function == nullptr || fragment_function == nullptr ||
      !vertex_function->IsValid() || !fragment_function->IsValid()) {
    LOGE("GPUDeviceVK::CreateRenderPipeline failed: invalid shader function");
    return {};
  }

  if (!desc.vertex_function->IsValid() || !desc.fragment_function->IsValid()) {
    return {};
  }

  std::vector<VkDescriptorSetLayout> set_layouts;
  GPURenderPipeline merged_pipeline(desc);
  if (!merged_pipeline.IsValid()) {
    LOGE(
        "GPUDeviceVK::CreateRenderPipeline failed: invalid merged bind groups");
    return {};
  }

  const auto bind_groups = merged_pipeline.GetBindGroups();
  uint32_t max_group = 0;
  for (const auto& bind_group : bind_groups) {
    max_group = std::max(max_group, bind_group.group);
  }
  if (!bind_groups.empty()) {
    set_layouts.resize(max_group + 1, VK_NULL_HANDLE);
    for (const auto& bind_group : bind_groups) {
      if (!CreateDescriptorSetLayout(*state_, bind_group,
                                     &set_layouts[bind_group.group])) {
        for (auto set_layout : set_layouts) {
          if (set_layout != VK_NULL_HANDLE) {
            state_->DeviceFns().vkDestroyDescriptorSetLayout(
                state_->GetLogicalDevice(), set_layout, nullptr);
          }
        }
        return {};
      }
    }

    for (auto& set_layout : set_layouts) {
      if (set_layout == VK_NULL_HANDLE &&
          !CreateDescriptorSetLayout(*state_, wgx::BindGroup{}, &set_layout)) {
        for (auto created_layout : set_layouts) {
          if (created_layout != VK_NULL_HANDLE) {
            state_->DeviceFns().vkDestroyDescriptorSetLayout(
                state_->GetLogicalDevice(), created_layout, nullptr);
          }
        }
        return {};
      }
    }
  }

  VkPipelineLayoutCreateInfo pipeline_layout_info = {};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount =
      static_cast<uint32_t>(set_layouts.size());
  pipeline_layout_info.pSetLayouts =
      set_layouts.empty() ? nullptr : set_layouts.data();

  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkResult result = state_->DeviceFns().vkCreatePipelineLayout(
      state_->GetLogicalDevice(), &pipeline_layout_info, nullptr,
      &pipeline_layout);
  if (result != VK_SUCCESS || pipeline_layout == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan pipeline layout: {}",
         static_cast<int32_t>(result));
    for (auto set_layout : set_layouts) {
      if (set_layout != VK_NULL_HANDLE) {
        state_->DeviceFns().vkDestroyDescriptorSetLayout(
            state_->GetLogicalDevice(), set_layout, nullptr);
      }
    }
    return {};
  }

  PipelineTargetInfo target_info = {};
  if (!BuildPipelineTargetInfo(*state_, desc, &target_info)) {
    state_->DeviceFns().vkDestroyPipelineLayout(state_->GetLogicalDevice(),
                                                pipeline_layout, nullptr);
    for (auto set_layout : set_layouts) {
      if (set_layout != VK_NULL_HANDLE) {
        state_->DeviceFns().vkDestroyDescriptorSetLayout(
            state_->GetLogicalDevice(), set_layout, nullptr);
      }
    }
    return {};
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {};
  shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[0].stage = ToVkShaderStage(vertex_function->GetStage());
  shader_stages[0].module = vertex_function->GetShaderModule();
  shader_stages[0].pName = vertex_function->GetEntryPoint().c_str();
  shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[1].stage = ToVkShaderStage(fragment_function->GetStage());
  shader_stages[1].module = fragment_function->GetShaderModule();
  shader_stages[1].pName = fragment_function->GetEntryPoint().c_str();

  std::vector<VkVertexInputBindingDescription> vertex_bindings;
  std::vector<VkVertexInputAttributeDescription> vertex_attributes;
  if (desc.buffers != nullptr) {
    vertex_bindings.reserve(desc.buffers->size());
    for (uint32_t i = 0; i < desc.buffers->size(); ++i) {
      const auto& buffer = (*desc.buffers)[i];
      VkVertexInputBindingDescription binding = {};
      binding.binding = i;
      binding.stride = static_cast<uint32_t>(buffer.array_stride);
      binding.inputRate = ToVkVertexInputRate(buffer.step_mode);
      vertex_bindings.push_back(binding);

      for (const auto& attribute : buffer.attributes) {
        VkVertexInputAttributeDescription vk_attribute = {};
        vk_attribute.location = attribute.shader_location;
        vk_attribute.binding = i;
        vk_attribute.format = ToVkVertexFormat(attribute.format);
        vk_attribute.offset = static_cast<uint32_t>(attribute.offset);
        vertex_attributes.push_back(vk_attribute);
      }
    }
  }

  VkPipelineVertexInputStateCreateInfo vertex_input_state = {};
  vertex_input_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_state.vertexBindingDescriptionCount =
      static_cast<uint32_t>(vertex_bindings.size());
  vertex_input_state.pVertexBindingDescriptions =
      vertex_bindings.empty() ? nullptr : vertex_bindings.data();
  vertex_input_state.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(vertex_attributes.size());
  vertex_input_state.pVertexAttributeDescriptions =
      vertex_attributes.empty() ? nullptr : vertex_attributes.data();

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {};
  input_assembly_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkViewport viewport = {};
  viewport.width = 1.f;
  viewport.height = 1.f;
  viewport.maxDepth = 1.f;

  VkRect2D scissor = {};
  scissor.extent.width = 1;
  scissor.extent.height = 1;

  VkPipelineViewportStateCreateInfo viewport_state = {};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  std::array<VkDynamicState, 5> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
      VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
      VK_DYNAMIC_STATE_STENCIL_REFERENCE,
  };
  VkPipelineDynamicStateCreateInfo dynamic_state = {};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount =
      static_cast<uint32_t>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  VkPipelineRasterizationStateCreateInfo rasterization_state = {};
  rasterization_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization_state.cullMode = VK_CULL_MODE_NONE;
  rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterization_state.lineWidth = 1.f;

  VkPipelineMultisampleStateCreateInfo multisample_state = {};
  multisample_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample_state.rasterizationSamples =
      static_cast<VkSampleCountFlagBits>(desc.sample_count);

  VkPipelineColorBlendAttachmentState color_blend_attachment = {};
  color_blend_attachment.blendEnable =
      desc.target.src_blend_factor != GPUBlendFactor::kOne ||
      desc.target.dst_blend_factor != GPUBlendFactor::kZero;
  color_blend_attachment.srcColorBlendFactor =
      ToVkBlendFactor(desc.target.src_blend_factor);
  color_blend_attachment.dstColorBlendFactor =
      ToVkBlendFactor(desc.target.dst_blend_factor);
  color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor =
      ToVkBlendFactor(desc.target.src_blend_factor);
  color_blend_attachment.dstAlphaBlendFactor =
      ToVkBlendFactor(desc.target.dst_blend_factor);
  color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  color_blend_attachment.colorWriteMask =
      ToVkColorWriteMask(desc.target.write_mask);

  VkPipelineColorBlendStateCreateInfo color_blend_state = {};
  color_blend_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend_state.attachmentCount = 1;
  color_blend_state.pAttachments = &color_blend_attachment;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
  depth_stencil_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state.depthTestEnable = desc.depth_stencil.enable_depth;
  depth_stencil_state.depthWriteEnable =
      desc.depth_stencil.enable_depth &&
      desc.depth_stencil.depth_state.enableWrite;
  depth_stencil_state.depthCompareOp =
      ToVkCompareOp(desc.depth_stencil.depth_state.compare);
  depth_stencil_state.stencilTestEnable = desc.depth_stencil.enable_stencil;
  depth_stencil_state.front =
      ToVkStencilOpState(desc.depth_stencil.stencil_state.front);
  depth_stencil_state.back =
      ToVkStencilOpState(desc.depth_stencil.stencil_state.back);

  VkGraphicsPipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  if (target_info.uses_dynamic_rendering) {
    pipeline_info.pNext = &target_info.rendering_info;
  }
  pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
  pipeline_info.pStages = shader_stages.data();
  pipeline_info.pVertexInputState = &vertex_input_state;
  pipeline_info.pInputAssemblyState = &input_assembly_state;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterization_state;
  pipeline_info.pMultisampleState = &multisample_state;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.pDepthStencilState =
      HasDepthStencilAttachmentFormat(desc.depth_stencil.format)
          ? &depth_stencil_state
          : nullptr;
  pipeline_info.pColorBlendState = &color_blend_state;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = target_info.render_pass;
  pipeline_info.subpass = 0;

  VkPipeline pipeline = VK_NULL_HANDLE;
  result = state_->DeviceFns().vkCreateGraphicsPipelines(
      state_->GetLogicalDevice(), state_->GetPipelineCache(), 1, &pipeline_info,
      nullptr, &pipeline);
  if (result != VK_SUCCESS || pipeline == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan graphics pipeline: {}",
         static_cast<int32_t>(result));
    state_->DeviceFns().vkDestroyPipelineLayout(state_->GetLogicalDevice(),
                                                pipeline_layout, nullptr);
    for (auto set_layout : set_layouts) {
      if (set_layout != VK_NULL_HANDLE) {
        state_->DeviceFns().vkDestroyDescriptorSetLayout(
            state_->GetLogicalDevice(), set_layout, nullptr);
      }
    }
    return {};
  }

  return std::make_unique<GPURenderPipelineVK>(
      state_, pipeline, pipeline_layout, target_info.render_pass,
      target_info.uses_dynamic_rendering, std::move(set_layouts), desc);
}

}  // namespace skity
