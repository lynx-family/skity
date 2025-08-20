// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_shader_function_vk.hpp"

#include <sstream>

#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/spirv_compiler_vk.hpp"
#include "src/logging.hpp"

namespace skity {

GPUShaderFunctionVk::GPUShaderFunctionVk(
    std::string label, GPUShaderStage stage,
    const std::vector<uint32_t>& spirv_code,
    const std::vector<int32_t>& constant_values,
    GPUShaderFunctionErrorCallback error_callback)
    : GPUShaderFunction(std::move(label)),
      spirv_code_(spirv_code),
      constant_values_(constant_values) {
  switch (stage) {
    case GPUShaderStage::kVertex:
      stage_ = VK_SHADER_STAGE_VERTEX_BIT;
      break;
    case GPUShaderStage::kFragment:
      stage_ = VK_SHADER_STAGE_FRAGMENT_BIT;
      break;
    default:
      stage_ = VK_SHADER_STAGE_VERTEX_BIT;
  }
}

GPUShaderFunctionVk::~GPUShaderFunctionVk() { Destroy(); }

std::shared_ptr<GPUShaderFunctionVk> GPUShaderFunctionVk::Create(
    GPUDeviceVk* device, const GPUShaderFunctionDescriptor& desc) {
  if (!device) {
    LOGE("Invalid device for shader function creation");
    return nullptr;
  }

  std::vector<uint32_t> spirv_code;

  // Create SPIRV compiler instance
  SPIRVCompilerVk spirv_compiler(device);

  if (desc.source_type == GPUShaderSourceType::kRaw) {
    auto* raw_source = static_cast<GPUShaderSourceRaw*>(desc.shader_source);
    if (!raw_source || !raw_source->source) {
      LOGE("Invalid shader source");
      return nullptr;
    }

    // Treat raw source as GLSL and compile to SPIRV
    SPIRVCompileOptions options = SPIRVUtils::GetDefaultOptions(desc.stage);
    auto compile_result = spirv_compiler.CompileGLSLToSPIRV(
        raw_source->source, desc.stage, options);

    if (!compile_result) {
      LOGE("GLSL to SPIRV compilation failed: %s",
           compile_result.error_message.c_str());
      return nullptr;
    }

    spirv_code = compile_result.spirv_code;
    LOGI("Successfully compiled GLSL to SPIRV (%zu words)", spirv_code.size());

  } else if (desc.source_type == GPUShaderSourceType::kWGX) {
    auto* wgx_source = static_cast<GPUShaderSourceWGX*>(desc.shader_source);
    if (!wgx_source || !wgx_source->module || !wgx_source->entry_point) {
      LOGE("Invalid WGX shader source");
      return nullptr;
    }

    // Get WGX program and convert to GLSL first
    auto* program = wgx_source->module->GetProgram();
    if (!program) {
      LOGE("Invalid WGX program");
      return nullptr;
    }

    // Configure GLSL options for Vulkan (GLSL 450)
    wgx::GlslOptions glsl_options;
    glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
    glsl_options.major_version = 4;
    glsl_options.minor_version = 5;

    // Convert WGX to GLSL
    auto wgx_result = program->WriteToGlsl(wgx_source->entry_point,
                                           glsl_options, wgx_source->context);
    if (!wgx_result.success) {
      LOGE("WGX to GLSL translation failed");
      return nullptr;
    }

    LOGI("WGX shader module (%s) translated function (%s) to GLSL successfully",
         wgx_source->module->GetLabel().c_str(), wgx_source->entry_point);

    // Compile GLSL to SPIRV
    SPIRVCompileOptions options = SPIRVUtils::GetDefaultOptions(desc.stage);
    auto compile_result = spirv_compiler.CompileGLSLToSPIRV(
        wgx_result.content, desc.stage, options);

    if (!compile_result) {
      LOGE("WGSL to SPIRV compilation failed: %s",
           compile_result.error_message.c_str());
      return nullptr;
    }

    spirv_code = compile_result.spirv_code;
    LOGI("Successfully compiled WGSL to SPIRV (%zu words)", spirv_code.size());

  } else {
    LOGE("Unsupported shader source type");
    return nullptr;
  }

  auto shader = std::make_shared<GPUShaderFunctionVk>(
      desc.label, desc.stage, spirv_code, desc.constant_values,
      desc.error_callback);

  // Set bind groups and WGX context for Vulkan shaders (like OpenGL version)
  if (desc.source_type == GPUShaderSourceType::kWGX) {
    auto* wgx_source = static_cast<GPUShaderSourceWGX*>(desc.shader_source);
    auto* program = wgx_source->module->GetProgram();
    wgx::GlslOptions glsl_options;
    glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
    glsl_options.major_version = 4;
    glsl_options.minor_version = 5;

    auto wgx_result = program->WriteToGlsl(wgx_source->entry_point,
                                           glsl_options, wgx_source->context);
    if (wgx_result.success) {
      shader->SetBindGroups(wgx_result.bind_groups);
      shader->SetWGXContext(wgx_result.context);
      wgx_source->context = wgx_result.context;
    }
  }

  if (!shader->Initialize(device)) {
    LOGE("Failed to initialize Vulkan shader function");
    return nullptr;
  }

  return shader;
}

bool GPUShaderFunctionVk::Initialize(GPUDeviceVk* device) {
  if (!device) {
    LOGE("Invalid device for shader initialization");
    return false;
  }

  if (spirv_code_.empty()) {
    LOGE("Empty SPIRV code");
    return false;
  }

  device_ = device;

  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = spirv_code_.size() * sizeof(uint32_t);
  create_info.pCode = spirv_code_.data();

  VkResult result = vkCreateShaderModule(device_->GetDevice(), &create_info,
                                         nullptr, &shader_module_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create shader module: %d", result);
    return false;
  }

  return true;
}

void GPUShaderFunctionVk::Destroy() {
  if (shader_module_ != VK_NULL_HANDLE && device_) {
    vkDestroyShaderModule(device_->GetDevice(), shader_module_, nullptr);
    shader_module_ = VK_NULL_HANDLE;
  }
  device_ = nullptr;
}

}  // namespace skity