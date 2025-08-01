// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/mtl/gpu_device_mtl.h"

#include <skity/macros.hpp>

#include "src/gpu/mtl/formats_mtl.h"
#include "src/gpu/mtl/gpu_buffer_mtl.h"
#include "src/gpu/mtl/gpu_command_buffer_mtl.h"
#include "src/gpu/mtl/gpu_render_pipeline_mtl.h"
#include "src/gpu/mtl/gpu_sampler_mtl.h"
#include "src/gpu/mtl/gpu_shader_function_mtl.h"
#include "src/gpu/mtl/gpu_texture_mtl.h"
#include "src/tracing.hpp"

// FIXME: spdlog seems to have some problem with fmt. when build in objc++
#ifdef FMT_EXCEPTIONS
#undef FMT_EXCEPTIONS
#endif
#define FMT_EXCEPTIONS 0
#include "src/logging.hpp"

namespace skity {

static bool SupportsMemoryless(id<MTLDevice> device) {
  // Refer to the "Memoryless render targets" feature in the table below:
  // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
  if (@available(ios 13.0, tvos 13.0, macos 10.15, *)) {
    return [device supportsFamily:MTLGPUFamilyApple2];
  } else {
#if defined(SKITY_IOS)
    // This is perhaps redundant. But, just in case we somehow get into a case
    // where Impeller runs on iOS versions less than 8.0 and/or without A8
    // GPUs, we explicitly check feature set support.
    return [device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1];
#else
    // MacOS devices with Apple GPUs are only available with macos 10.15 and
    // above. So, if we are here, it is safe to assume that memory-less targets
    // are not supported.
    return false;
#endif
  }
}

GPUDeviceMTL::GPUDeviceMTL(id<MTLDevice> device, id<MTLCommandQueue> queue)
    : GPUDevice(),
      mtl_device_(device),
      mtl_command_queue_(queue),
      supports_memoryless_(SupportsMemoryless(device)) {}

GPUDeviceMTL::~GPUDeviceMTL() = default;

std::unique_ptr<GPUBuffer> GPUDeviceMTL::CreateBuffer(GPUBufferUsageMask usage) {
  return std::make_unique<GPUBufferMTL>(usage, mtl_device_, mtl_command_queue_);
}

std::shared_ptr<GPUShaderFunction> GPUDeviceMTL::CreateShaderFunction(
    const GPUShaderFunctionDescriptor& desc) {
  if (desc.source_type == GPUShaderSourceType::kWGX) {
    return CreateShaderFunctionFromModule(desc);
  }

  const GPUShaderSourceRaw* source =
      reinterpret_cast<const GPUShaderSourceRaw*>(desc.shader_source);

  auto function = std::make_shared<GPUShaderFunctionMTL>(desc.label, mtl_device_, desc.stage,
                                                         source->source, source->entry_point,
                                                         desc.constant_values, desc.error_callback);
  if (!function->IsValid()) {
    return nullptr;
  }
  return function;
}

id<MTLDepthStencilState> GPUDeviceMTL::FindOrCreateDepthStencilState(
    const GPUDepthStencilState& depth_stencil) {
  id<MTLDepthStencilState> depthStencilState;
  auto it = depth_stencil_map_.find(depth_stencil);
  if (it != depth_stencil_map_.end()) {
    depthStencilState = it->second;
  } else {
    MTLDepthStencilDescriptor* depthStencilDesc = ToMTLDepthStencilDescriptor(depth_stencil);
    depthStencilState = [mtl_device_ newDepthStencilStateWithDescriptor:depthStencilDesc];
    depth_stencil_map_.insert({depth_stencil, depthStencilState});
  }
  return depthStencilState;
}

std::unique_ptr<GPURenderPipeline> GPUDeviceMTL::CreateRenderPipeline(
    const GPURenderPipelineDescriptor& desc) {
  if (desc.vertex_function == nullptr || desc.fragment_function == nullptr) {
    return {};
  }

  auto pipeline = GPURenderPipelineMTL::Make(*this, desc);
  if (!pipeline->IsValid()) {
    return nullptr;
  }
  return pipeline;
}

std::unique_ptr<GPURenderPipeline> GPUDeviceMTL::ClonePipeline(
    GPURenderPipeline* base, const skity::GPURenderPipelineDescriptor& desc) {
  if (!base->IsValid()) {
    return std::unique_ptr<GPURenderPipeline>();
  }

  auto pipeline_mtl = static_cast<GPURenderPipelineMTL*>(base);

  // in Metal if blending factor is same, we can reuse this pipeline and only
  // create different MTLStencilState, and we can share these MTLStencilState if
  // needed
  if (pipeline_mtl->GetDescriptor().depth_stencil.format == desc.depth_stencil.format &&
      pipeline_mtl->GetDescriptor().target == desc.target &&
      pipeline_mtl->GetDescriptor().sample_count == desc.sample_count) {
    id<MTLDepthStencilState> depthStencilState = FindOrCreateDepthStencilState(desc.depth_stencil);

    return std::make_unique<GPURenderPipelineMTL>(pipeline_mtl->GetMTLRenderPipelineState(),
                                                  depthStencilState, desc);
  }

  // create a new pipeline with other blending mode
  return CreateRenderPipeline(desc);
}

std::shared_ptr<GPUCommandBuffer> GPUDeviceMTL::CreateCommandBuffer() {
  return std::make_shared<GPUCommandBufferMTL>(mtl_device_, [mtl_command_queue_ commandBuffer]);
}

std::shared_ptr<GPUSampler> GPUDeviceMTL::CreateSampler(const GPUSamplerDescriptor& desc) {
  auto it = sampler_map_.find(desc);
  if (it != sampler_map_.end()) {
    return it->second;
  }
  auto sampler = GPUSamplerMTL::Create(*this, desc);
  sampler_map_.insert({desc, sampler});
  return sampler;
}

std::shared_ptr<GPUTexture> GPUDeviceMTL::CreateTexture(const GPUTextureDescriptor& desc) {
  return GPUTextureMTL::Create(*this, desc);
}

bool GPUDeviceMTL::CanUseMSAA() {
  // Metal can always enable msaa
  return true;
}

uint32_t GPUDeviceMTL::GetBufferAlignment() {
#if defined(SKITY_IOS) && !TARGET_OS_SIMULATOR
  return 16;
#else
  return 256;
#endif
}

uint32_t GPUDeviceMTL::GetMaxTextureSize() {
  if (max_texture_size_ == 0) {
    uint32_t max_tex_size = 4096;
#if defined(SKITY_IOS)
    if (@available(iOS 11.0, *)) {
      if ([mtl_device_ supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1]) {
        max_tex_size = 16384;
      }
    } else if ([mtl_device_ supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1]) {
      max_tex_size = 16384;
    } else if ([mtl_device_ supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v2] ||
               [mtl_device_ supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v2]) {
      max_tex_size = 8192;
    } else {
      max_tex_size = 4096;
    }
#else
    if ([mtl_device_ supportsFamily:MTLGPUFamilyApple3]) {
      max_tex_size = 16384;
    } else if ([mtl_device_ supportsFamily:MTLGPUFamilyApple2]) {
      max_tex_size = 8192;
    } else {
      max_tex_size = 4096;
    }
#endif

    max_texture_size_ = max_tex_size;
  }

  return max_texture_size_;
}

std::shared_ptr<GPUShaderFunction> GPUDeviceMTL::CreateShaderFunctionFromModule(
    const GPUShaderFunctionDescriptor& desc) {
  SKITY_TRACE_EVENT(GPUDeviceMTL_CreateShaderFunctionFromModuleWGX);

  if (desc.source_type != GPUShaderSourceType::kWGX) {
    return {};
  }

  GPUShaderSourceWGX* source = reinterpret_cast<GPUShaderSourceWGX*>(desc.shader_source);

  if (!source->module || source->module->GetProgram() == nullptr ||
      source->entry_point == nullptr) {
    return {};
  }

  wgx::MslOptions options{};

  // skity target iOS 11.0 and above. make sure we use msl 2.0
  options.msl_version_major = 2;
  options.msl_version_minor = 0;

  auto wgx_result =
      source->module->GetProgram()->WriteToMsl(source->entry_point, options, source->context);

  if (!wgx_result.success) {
    if (desc.error_callback) {
      desc.error_callback("WGX translate error");
    }
    return {};
  }

  LOGD("WGX shader_module ( {} ) translate function ( {} ) result: {}", source->module->GetLabel(),
       source->entry_point, wgx_result.content);

  auto function = std::make_shared<GPUShaderFunctionMTL>(
      desc.label, mtl_device_, desc.stage, wgx_result.content.c_str(), source->entry_point,
      std::vector<int32_t>{}, desc.error_callback);

  if (!function->IsValid()) {
    return {};
  }

  function->SetBindGroups(wgx_result.bind_groups);
  function->SetWGXContext(wgx_result.context);

  // pass the wgx context to caller
  source->context = wgx_result.context;

  return function;
}

}  // namespace skity
