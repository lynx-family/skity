// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_shader_module_web.hpp"

#include "src/logging.hpp"

namespace skity {

namespace {

void ShaderCompileCallback(WGPUCompilationInfoRequestStatus,
                           struct WGPUCompilationInfo const* compilationInfo,
                           void*, void*) {
  for (size_t i = 0; i < compilationInfo->messageCount; i++) {
    auto message = compilationInfo->messages + i;

    if (message->type == WGPUCompilationMessageType_Error) {
      LOGE("Shader compile error at line {} column {}: {}", message->lineNum,
           message->linePos,
           std::string(message->message.data, message->message.length));
    } else if (message->type == WGPUCompilationMessageType_Warning) {
      LOGW("Shader compile warning at line {} column {}: {}", message->lineNum,
           message->linePos,
           std::string(message->message.data, message->message.length));
    } else {
      LOGI("Shader compile info at line {} column {}: {}", message->lineNum,
           message->linePos,
           std::string(message->message.data, message->message.length));
    }
  }
}

}  // namespace

GPUShaderModuleWEB::GPUShaderModuleWEB(GPUShaderModule& base,
                                       WGPUShaderModule shader_module)
    : GPUShaderModule(base), shader_module_(shader_module) {}

GPUShaderModuleWEB::~GPUShaderModuleWEB() {
  wgpuShaderModuleRelease(shader_module_);
}

std::shared_ptr<GPUShaderModule> GPUShaderModuleWEB::Create(
    WGPUDevice device, const GPUShaderModuleDescriptor& desc) {
  // we needs to reflect the WGSL source code for uploading data in the future
  // this may cause some redundant work, but the webgpu doesn't provide a way
  // to get the reflect info from WGPUShaderModule
  auto base_module = GPUShaderModule::Create(desc);

  if (!base_module) {
    return {};
  }

  WGPUShaderModuleDescriptor wgpu_desc = {};
  wgpu_desc.label.data = desc.label.c_str();
  wgpu_desc.label.length = desc.label.length();

  WGPUShaderSourceWGSL wgpu_source = WGPU_SHADER_SOURCE_WGSL_INIT;
  wgpu_source.code.data = desc.source.c_str();
  wgpu_source.code.length = desc.source.length();

  wgpu_desc.nextInChain = &wgpu_source.chain;

  WGPUShaderModule shader_module =
      wgpuDeviceCreateShaderModule(device, &wgpu_desc);

  if (!shader_module) {
    return {};
  }

#ifdef SKITY_LOG
  wgpuShaderModuleGetCompilationInfo(shader_module,
                                     WGPUCompilationInfoCallbackInfo{
                                         nullptr,
                                         WGPUCallbackMode_AllowSpontaneous,
                                         ShaderCompileCallback,
                                         nullptr,
                                         nullptr,
                                     });
#endif

  return std::make_shared<GPUShaderModuleWEB>(*base_module, shader_module);
}

}  // namespace skity
