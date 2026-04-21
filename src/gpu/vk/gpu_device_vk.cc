// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_device_vk.hpp"

#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/gpu_shader_function_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"
#include "src/tracing.hpp"

namespace skity {

namespace {

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
  (void)desc;
  LOGW("GPUDeviceVK::CreateRenderPipeline is not implemented yet");
  return {};
}

std::unique_ptr<GPURenderPipeline> GPUDeviceVK::ClonePipeline(
    GPURenderPipeline* base, const GPURenderPipelineDescriptor& desc) {
  (void)base;
  (void)desc;
  LOGW("GPUDeviceVK::ClonePipeline is not implemented yet");
  return {};
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
  (void)desc;
  LOGW("GPUDeviceVK::CreateSampler is not implemented yet");
  return {};
}

std::shared_ptr<GPUTexture> GPUDeviceVK::CreateTexture(
    const GPUTextureDescriptor& desc) {
  (void)desc;
  LOGW("GPUDeviceVK::CreateTexture is not implemented yet");
  return {};
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

}  // namespace skity
