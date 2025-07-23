// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/mtl/gpu_render_pipeline_mtl.h"

#include <cassert>
#include <memory>

#include "src/gpu/mtl/formats_mtl.h"
#include "src/gpu/mtl/gpu_device_mtl.h"
#include "src/gpu/mtl/gpu_shader_function_mtl.h"

namespace skity {

std::unique_ptr<GPURenderPipelineMTL> GPURenderPipelineMTL::Make(
    GPUDeviceMTL& device, const GPURenderPipelineDescriptor& desc) {
  MTLRenderPipelineDescriptor* render_pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];

  id<MTLFunction> vertex_function =
      static_cast<GPUShaderFunctionMTL*>(desc.vertex_function.get())->GetMTLFunction();
  id<MTLFunction> fragment_function =
      static_cast<GPUShaderFunctionMTL*>(desc.fragment_function.get())->GetMTLFunction();
  render_pipeline_desc.vertexFunction = vertex_function;
  render_pipeline_desc.fragmentFunction = fragment_function;
  render_pipeline_desc.vertexDescriptor = ToMTLVertexDescriptor(desc.buffers);

  if (@available(macOS 10.13, iOS 11.0, *)) {
    render_pipeline_desc.vertexBuffers[0].mutability = MTLMutabilityImmutable;
  }

  if (@available(macOS 13.0, iOS 16.0, *)) {
    render_pipeline_desc.rasterSampleCount = desc.sample_count;
  } else {
    render_pipeline_desc.sampleCount = desc.sample_count;
  }

  if (desc.depth_stencil.format == GPUTextureFormat::kDepth24Stencil8) {
    render_pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    render_pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
  } else if (desc.depth_stencil.format == GPUTextureFormat::kStencil8) {
    render_pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatStencil8;
  }

  auto& target = desc.target;
  render_pipeline_desc.colorAttachments[0].pixelFormat = ToMTLTextureFormat(target.format);
  render_pipeline_desc.colorAttachments[0].blendingEnabled =
      !(target.src_blend_factor == GPUBlendFactor::kOne &&
        target.dst_blend_factor == GPUBlendFactor::kZero);

  MTLBlendFactor src = ToMTLBlendFactor(target.src_blend_factor);
  MTLBlendFactor dst = ToMTLBlendFactor(target.dst_blend_factor);

  render_pipeline_desc.colorAttachments[0].sourceAlphaBlendFactor = src;
  render_pipeline_desc.colorAttachments[0].sourceRGBBlendFactor = src;
  render_pipeline_desc.colorAttachments[0].destinationAlphaBlendFactor = dst;
  render_pipeline_desc.colorAttachments[0].destinationRGBBlendFactor = dst;

  render_pipeline_desc.colorAttachments[0].writeMask =
      static_cast<MTLColorWriteMask>(MTLColorWriteMaskAll & target.write_mask);
  render_pipeline_desc.label = @(desc.label.c_str());

  NSError* psoError;
  id<MTLRenderPipelineState> render_pipeline_state =
      [device.GetMTLDevice() newRenderPipelineStateWithDescriptor:render_pipeline_desc
                                                            error:&psoError];

  id<MTLDepthStencilState> depthStencilState =
      device.FindOrCreateDepthStencilState(desc.depth_stencil);

  return std::make_unique<GPURenderPipelineMTL>(render_pipeline_state, depthStencilState, desc);
}

}  // namespace skity
