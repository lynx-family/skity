// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_device_web.hpp"

#include "src/gpu/web/gpu_buffer_web.hpp"
#include "src/gpu/web/gpu_command_buffer_web.hpp"
#include "src/gpu/web/gpu_render_pipeline_web.hpp"
#include "src/gpu/web/gpu_sampler_web.hpp"
#include "src/gpu/web/gpu_shader_function_web.hpp"
#include "src/gpu/web/gpu_shader_module_web.hpp"
#include "src/gpu/web/gpu_texture_web.hpp"

namespace skity {

GPUDeviceWEB::GPUDeviceWEB(WGPUDevice device, WGPUQueue queue)
    : device_(device), queue_(queue), limits_() {
  wgpuDeviceGetLimits(device_, &limits_);
}

std::unique_ptr<GPUBuffer> GPUDeviceWEB::CreateBuffer(
    GPUBufferUsageMask usage) {
  return std::make_unique<GPUBufferWEB>(usage);
}

std::shared_ptr<GPUShaderFunction> GPUDeviceWEB::CreateShaderFunction(
    const GPUShaderFunctionDescriptor& desc) {
  if (desc.source_type != GPUShaderSourceType::kWGX) {
    return {};
  }

  auto shader_source =
      reinterpret_cast<const GPUShaderSourceWGX*>(desc.shader_source);

  if (shader_source == nullptr || shader_source->module == nullptr) {
    return {};
  }

  auto wgpu_module =
      dynamic_cast<const GPUShaderModuleWEB*>(shader_source->module.get());

  if (!wgpu_module) {
    return {};
  }

  auto groups =
      wgpu_module->GetProgram()->GetWGSLBindGroups(shader_source->entry_point);

  auto wgpu_function = std::make_shared<GPUShaderFunctionWeb>(
      desc.label, shader_source->entry_point, wgpu_module->GetShaderModule());

  wgpu_function->SetBindGroups(groups);

  return wgpu_function;
}

std::unique_ptr<GPURenderPipeline> GPUDeviceWEB::CreateRenderPipeline(
    const GPURenderPipelineDescriptor& desc) {
  return GPURenderPipelineWeb::Create(device_, desc);
}

std::unique_ptr<GPURenderPipeline> GPUDeviceWEB::ClonePipeline(
    GPURenderPipeline* base, const GPURenderPipelineDescriptor& desc) {
  // all depth stencil state and blend state are immutable in webgpu
  // just recreate a new pipeline
  return GPURenderPipelineWeb::Create(device_, desc);
}

std::shared_ptr<GPUCommandBuffer> GPUDeviceWEB::CreateCommandBuffer() {
  WGPUCommandEncoderDescriptor desc = {};

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device_, &desc);

  if (!encoder) {
    return {};
  }

  return std::make_shared<GPUCommandBufferWEB>(device_, queue_, encoder);
}

std::shared_ptr<GPUSampler> GPUDeviceWEB::CreateSampler(
    const GPUSamplerDescriptor& desc) {
  return GPUSamplerWEB::Create(device_, desc);
}

std::shared_ptr<GPUTexture> GPUDeviceWEB::CreateTexture(
    const GPUTextureDescriptor& desc) {
  return GPUTextureWEB::Create(this, desc);
}

std::shared_ptr<GPUShaderModule> GPUDeviceWEB::CreateShaderModule(
    const GPUShaderModuleDescriptor& desc) {
  return GPUShaderModuleWEB::Create(device_, desc);
}

}  // namespace skity
