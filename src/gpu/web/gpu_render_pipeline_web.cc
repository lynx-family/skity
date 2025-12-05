// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_render_pipeline_web.hpp"

#include "src/gpu/web/format_web.hpp"
#include "src/gpu/web/gpu_shader_function_web.hpp"

namespace skity {

namespace {

struct VertexBufferLayout {
  WGPUVertexBufferLayout layout;
  std::vector<WGPUVertexAttribute> attributes;

  explicit VertexBufferLayout(const GPUVertexBufferLayout& l) {
    layout.arrayStride = l.array_stride;
    if (l.step_mode == GPUVertexStepMode::kVertex) {
      layout.stepMode = WGPUVertexStepMode_Vertex;
    } else {
      layout.stepMode = WGPUVertexStepMode_Instance;
    }

    for (const auto& attr : l.attributes) {
      WGPUVertexAttribute wgpu_attr = {};
      wgpu_attr.format = ToWGPUVertexFormat(attr.format);
      wgpu_attr.offset = attr.offset;
      wgpu_attr.shaderLocation = attr.shader_location;

      attributes.emplace_back(wgpu_attr);
    }

    layout.attributeCount = attributes.size();
    layout.attributes = attributes.data();
  }
};

struct FragmentState {
  WGPUFragmentState state = {};
  WGPUColorTargetState target = {};
  WGPUBlendState blend = {};

  explicit FragmentState(const GPUColorTargetState& fragment) {
    state.targetCount = 1;
    state.targets = &target;

    target.format = ToWGPUTextureFormat(fragment.format);
    target.writeMask = fragment.write_mask != 0 ? WGPUColorWriteMask_All
                                                : WGPUColorWriteMask_None;

    if (fragment.src_blend_factor == GPUBlendFactor::kOne &&
        fragment.dst_blend_factor == GPUBlendFactor::kZero) {
      // disable blend
      target.blend = nullptr;
    } else {
      target.blend = &blend;

      blend.color.operation = WGPUBlendOperation_Add;
      blend.alpha.operation = WGPUBlendOperation_Add;

      blend.color.srcFactor = blend.alpha.srcFactor =
          ToWGPUBlendFactor(fragment.src_blend_factor);
      blend.color.dstFactor = blend.alpha.dstFactor =
          ToWGPUBlendFactor(fragment.dst_blend_factor);
    }
  }
};

void DefaultPrimitiveTopology(WGPUPrimitiveState& status) {
  status.topology = WGPUPrimitiveTopology_TriangleList;
  status.stripIndexFormat = WGPUIndexFormat_Undefined;
  status.cullMode = WGPUCullMode_None;
  status.frontFace = WGPUFrontFace_CW;
  status.unclippedDepth = false;
}

WGPUDepthStencilState GetDepthStencilStatus(
    const GPURenderPipelineDescriptor& desc) {
  WGPUDepthStencilState status = WGPU_DEPTH_STENCIL_STATE_INIT;

  status.format = ToWGPUTextureFormat(desc.depth_stencil.format);

  if (desc.depth_stencil.enable_depth) {
    status.depthWriteEnabled = desc.depth_stencil.depth_state.enableWrite
                                   ? WGPUOptionalBool_True
                                   : WGPUOptionalBool_False;
    status.depthCompare =
        ToWGPUCompareFunction(desc.depth_stencil.depth_state.compare);
    status.depthBias = 0.f;
    status.depthBiasSlopeScale = 1.f;
    status.depthBiasClamp = 1.f;
  } else {
    status.depthWriteEnabled = WGPUOptionalBool_False;
    status.depthCompare = WGPUCompareFunction_Undefined;
  }

  if (desc.depth_stencil.enable_stencil) {
    status.stencilFront.compare =
        ToWGPUCompareFunction(desc.depth_stencil.stencil_state.front.compare);
    status.stencilFront.failOp =
        ToWGPUStencilOperation(desc.depth_stencil.stencil_state.front.fail_op);
    status.stencilFront.depthFailOp = ToWGPUStencilOperation(
        desc.depth_stencil.stencil_state.front.depth_fail_op);
    status.stencilFront.passOp =
        ToWGPUStencilOperation(desc.depth_stencil.stencil_state.front.pass_op);

    status.stencilBack.compare =
        ToWGPUCompareFunction(desc.depth_stencil.stencil_state.back.compare);
    status.stencilBack.failOp =
        ToWGPUStencilOperation(desc.depth_stencil.stencil_state.back.fail_op);
    status.stencilBack.depthFailOp = ToWGPUStencilOperation(
        desc.depth_stencil.stencil_state.back.depth_fail_op);
    status.stencilBack.passOp =
        ToWGPUStencilOperation(desc.depth_stencil.stencil_state.back.pass_op);

    status.stencilReadMask =
        desc.depth_stencil.stencil_state.front.stencil_read_mask & 0xFF;
    status.stencilWriteMask =
        desc.depth_stencil.stencil_state.front.stencil_write_mask & 0xFF;
  } else {
    status.stencilFront.compare = status.stencilBack.compare =
        WGPUCompareFunction_Always;
    status.stencilFront.failOp = status.stencilBack.failOp =
        WGPUStencilOperation_Keep;
    status.stencilFront.depthFailOp = status.stencilBack.depthFailOp =
        WGPUStencilOperation_Keep;
    status.stencilFront.passOp = status.stencilBack.passOp =
        WGPUStencilOperation_Keep;
    status.stencilReadMask = status.stencilWriteMask = 0;
  }

  return status;
}

}  // namespace

GPURenderPipelineWeb::GPURenderPipelineWeb(
    const GPURenderPipelineDescriptor& desc, WGPURenderPipeline pipeline)
    : GPURenderPipeline(desc), pipeline_(pipeline) {}

GPURenderPipelineWeb::~GPURenderPipelineWeb() {
  wgpuRenderPipelineRelease(pipeline_);
}

std::unique_ptr<GPURenderPipeline> GPURenderPipelineWeb::Create(
    WGPUDevice device, const GPURenderPipelineDescriptor& desc) {
  WGPURenderPipelineDescriptor wgpu_desc = {};

  wgpu_desc.label.data = desc.label.c_str();
  wgpu_desc.label.length = desc.label.size();

  auto vs_function =
      dynamic_cast<const GPUShaderFunctionWeb*>(desc.vertex_function.get());

  auto fs_function =
      dynamic_cast<const GPUShaderFunctionWeb*>(desc.fragment_function.get());

  if (!vs_function || !fs_function) {
    return {};
  }

  wgpu_desc.vertex.module = vs_function->GetShaderModule();
  wgpu_desc.vertex.entryPoint.data = vs_function->GetEntryPoint().c_str();
  wgpu_desc.vertex.entryPoint.length = vs_function->GetEntryPoint().size();

  std::vector<VertexBufferLayout> buffer_layouts;
  for (const auto& layout : desc.buffers) {
    buffer_layouts.emplace_back(layout);
  }

  std::vector<WGPUVertexBufferLayout> wgpu_buffer_layouts;
  for (const auto& layout : buffer_layouts) {
    wgpu_buffer_layouts.push_back(layout.layout);
  }

  wgpu_desc.vertex.bufferCount = wgpu_buffer_layouts.size();
  wgpu_desc.vertex.buffers = wgpu_buffer_layouts.data();

  DefaultPrimitiveTopology(wgpu_desc.primitive);

  auto depth_stencil = GetDepthStencilStatus(desc);

  if (depth_stencil.format != WGPUTextureFormat_Undefined) {
    wgpu_desc.depthStencil = &depth_stencil;
  }

  wgpu_desc.multisample.count = desc.sample_count;
  wgpu_desc.multisample.mask = ~0u;
  wgpu_desc.multisample.alphaToCoverageEnabled = false;
  FragmentState fragment{desc.target};

  fragment.state.module = fs_function->GetShaderModule();
  fragment.state.entryPoint.data = fs_function->GetEntryPoint().c_str();
  fragment.state.entryPoint.length = fs_function->GetEntryPoint().size();

  wgpu_desc.fragment = &fragment.state;

  auto pipeline = wgpuDeviceCreateRenderPipeline(device, &wgpu_desc);

  if (!pipeline) {
    return {};
  }

  return std::make_unique<GPURenderPipelineWeb>(desc, pipeline);
}

}  // namespace skity
