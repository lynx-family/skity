// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_render_pass_web.hpp"

#include "src/gpu/web/format_web.hpp"
#include "src/gpu/web/gpu_buffer_web.hpp"
#include "src/gpu/web/gpu_command_buffer_web.hpp"
#include "src/gpu/web/gpu_render_pipeline_web.hpp"
#include "src/gpu/web/gpu_sampler_web.hpp"
#include "src/gpu/web/gpu_texture_web.hpp"

namespace skity {

namespace {

struct BindGroupInfo {
  uint32_t group = 0;
  std::vector<WGPUBindGroupEntry> entries;

  void AddBindGroup(const UniformBinding& binding) {
    WGPUBindGroupEntry entry = {};

    entry.binding = binding.binding;
    entry.buffer =
        dynamic_cast<GPUBufferWEB*>(binding.buffer.buffer)->GetBuffer();
    entry.offset = binding.buffer.offset;
    entry.size = binding.buffer.range;

    entries.emplace_back(entry);
  }

  void AddBindGroup(const TextureBinding& binding) {
    WGPUBindGroupEntry entry = {};

    entry.binding = binding.binding;
    entry.textureView =
        dynamic_cast<GPUTextureWEB*>(binding.texture.get())->GetTextureView();

    entries.emplace_back(entry);
  }

  void AddBindGroup(const SamplerBinding& binding) {
    WGPUBindGroupEntry entry = {};

    entry.binding = binding.binding;
    entry.sampler =
        dynamic_cast<GPUSamplerWEB*>(binding.sampler.get())->GetSampler();

    entries.emplace_back(entry);
  }

  WGPUBindGroup CreateBindGroup(WGPUDevice device, WGPUBindGroupLayout layout) {
    if (entries.empty()) {
      return nullptr;
    }

    WGPUBindGroupDescriptor desc = {};
    desc.layout = layout;
    desc.entryCount = static_cast<uint32_t>(entries.size());
    desc.entries = entries.data();

    return wgpuDeviceCreateBindGroup(device, &desc);
  }
};

struct BindGroupContext {
  std::vector<BindGroupInfo> bind_groups;

  void GenInfo(const Command* command) {
    for (const auto& binding : command->uniform_bindings) {
      GetOrCreateBindGroupInfo(binding.group).AddBindGroup(binding);
    }

    for (const auto& binding : command->texture_bindings) {
      GetOrCreateBindGroupInfo(binding.group).AddBindGroup(binding);
    }

    for (const auto& binding : command->sampler_bindings) {
      GetOrCreateBindGroupInfo(binding.group).AddBindGroup(binding);
    }
  }

 private:
  BindGroupInfo& GetOrCreateBindGroupInfo(uint32_t group) {
    for (auto& info : bind_groups) {
      if (info.group == group) {
        return info;
      }
    }

    bind_groups.emplace_back(BindGroupInfo{group, {}});

    return bind_groups.back();
  }
};

}  // namespace

GPURenderPassWEB::GPURenderPassWEB(const GPURenderPassDescriptor& desc,
                                   GPUCommandBufferWEB* command_buffer,
                                   WGPUDevice device,
                                   WGPUCommandEncoder encoder)
    : GPURenderPass(desc),
      command_buffer_(command_buffer),
      device_(device),
      encoder_(encoder) {
  WGPUProcCommandEncoderAddRef(encoder_);
}

GPURenderPassWEB::~GPURenderPassWEB() {
  WGPUProcCommandEncoderRelease(encoder_);
}

void GPURenderPassWEB::EncodeCommands(std::optional<GPUViewport> viewport,
                                      std::optional<GPUScissorRect> scissor) {
  WGPURenderPassEncoder render_pass = BeginRenderPass();

  if (render_pass == nullptr) {
    return;
  }

  auto target_width = GetDescriptor().GetTargetWidth();
  auto target_height = GetDescriptor().GetTargetHeight();

  GPUViewport v = viewport.value_or(GPUViewport{
      0,
      0,
      static_cast<float>(target_width),
      static_cast<float>(target_height),
      0,
      1,
  });
  GPUScissorRect s = scissor.value_or(GPUScissorRect{
      0,
      0,
      target_width,
      target_height,
  });

  wgpuRenderPassEncoderSetViewport(render_pass, v.x, v.y, v.width, v.height,
                                   v.min_depth, v.max_depth);
  wgpuRenderPassEncoderSetScissorRect(render_pass, s.x, s.y, s.width, s.height);

  for (auto& command : GetCommands()) {
    if (!command->IsValid()) {
      continue;
    }

    wgpuRenderPassEncoderSetScissorRect(
        render_pass, command->scissor_rect.x, command->scissor_rect.y,
        command->scissor_rect.width, command->scissor_rect.height);

    auto pipeline = dynamic_cast<GPURenderPipelineWeb*>(command->pipeline)
                        ->GetRenderPipeline();

    wgpuRenderPassEncoderSetPipeline(render_pass, pipeline);

    // setup vertex buffer
    wgpuRenderPassEncoderSetVertexBuffer(
        render_pass, 0,
        dynamic_cast<GPUBufferWEB*>(command->vertex_buffer.buffer)->GetBuffer(),
        command->vertex_buffer.offset, command->vertex_buffer.range);

    if (command->IsInstanced()) {
      wgpuRenderPassEncoderSetVertexBuffer(
          render_pass, 1,
          dynamic_cast<GPUBufferWEB*>(command->instance_buffer.buffer)
              ->GetBuffer(),
          command->instance_buffer.offset, command->instance_buffer.range);
    }

    // setup index buffer
    wgpuRenderPassEncoderSetIndexBuffer(
        render_pass,
        dynamic_cast<GPUBufferWEB*>(command->index_buffer.buffer)->GetBuffer(),
        WGPUIndexFormat_Uint32, command->index_buffer.offset,
        command->index_buffer.range);

    // setup bind group
    SetupBindGroup(render_pass, command);

    wgpuRenderPassEncoderSetStencilReference(render_pass,
                                             command->stencil_reference);
    // draw
    wgpuRenderPassEncoderDrawIndexed(
        render_pass, command->index_count,
        command->instance_count == 0 ? 1 : command->instance_count, 0, 0, 0);
  }

  wgpuRenderPassEncoderEnd(render_pass);
}

WGPURenderPassEncoder GPURenderPassWEB::BeginRenderPass() {
  const auto& desc = GetDescriptor();

  WGPURenderPassDescriptor desc_web = {};

  WGPURenderPassColorAttachment color_attachment = {};
  color_attachment.loadOp = ToWGPULoadOp(desc.color_attachment.load_op);
  color_attachment.storeOp = ToWGPUStoreOp(desc.color_attachment.store_op);
  color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
  color_attachment.clearValue = ToWGPUColor(desc.color_attachment.clear_value);

  color_attachment.view =
      dynamic_cast<GPUTextureWEB*>(desc.color_attachment.texture.get())
          ->GetTextureView();

  if (desc.color_attachment.resolve_texture != nullptr) {
    color_attachment.resolveTarget =
        dynamic_cast<GPUTextureWEB*>(
            desc.color_attachment.resolve_texture.get())
            ->GetTextureView();
  }

  desc_web.colorAttachmentCount = 1;
  desc_web.colorAttachments = &color_attachment;

  WGPURenderPassDepthStencilAttachment depth_stencil_attachment =
      WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;

  if (desc.depth_attachment.texture == nullptr &&
      desc.stencil_attachment.texture == nullptr) {
    desc_web.depthStencilAttachment = nullptr;
  } else {
    desc_web.depthStencilAttachment = &depth_stencil_attachment;

    if (desc.stencil_attachment.texture != nullptr) {
      depth_stencil_attachment.stencilLoadOp =
          ToWGPULoadOp(desc.stencil_attachment.load_op);
      depth_stencil_attachment.stencilStoreOp =
          ToWGPUStoreOp(desc.stencil_attachment.store_op);
      depth_stencil_attachment.stencilReadOnly = WGPU_FALSE;
      depth_stencil_attachment.stencilClearValue =
          desc.stencil_attachment.clear_value;
      depth_stencil_attachment.view =
          dynamic_cast<GPUTextureWEB*>(desc.stencil_attachment.texture.get())
              ->GetTextureView();
    } else {
      depth_stencil_attachment.stencilLoadOp = WGPULoadOp_Undefined;
      depth_stencil_attachment.stencilStoreOp = WGPUStoreOp_Undefined;
    }

    if (desc.depth_attachment.texture != nullptr) {
      depth_stencil_attachment.depthLoadOp =
          ToWGPULoadOp(desc.depth_attachment.load_op);
      depth_stencil_attachment.depthStoreOp =
          ToWGPUStoreOp(desc.depth_attachment.store_op);
      depth_stencil_attachment.depthReadOnly = false;
      depth_stencil_attachment.depthClearValue =
          desc.depth_attachment.clear_value;
      depth_stencil_attachment.view =
          dynamic_cast<GPUTextureWEB*>(desc.depth_attachment.texture.get())
              ->GetTextureView();
    } else {
      depth_stencil_attachment.depthClearValue = 0.0f;
      depth_stencil_attachment.depthLoadOp = WGPULoadOp_Undefined;
      depth_stencil_attachment.depthStoreOp = WGPUStoreOp_Undefined;
    }
  }

  return wgpuCommandEncoderBeginRenderPass(encoder_, &desc_web);
}

void GPURenderPassWEB::SetupBindGroup(WGPURenderPassEncoder render_pass,
                                      const Command* command) {
  auto pipeline = dynamic_cast<GPURenderPipelineWeb*>(command->pipeline);

  if (pipeline == nullptr || pipeline->GetRenderPipeline() == nullptr) {
    return;
  }

  BindGroupContext ctx;
  ctx.GenInfo(command);

  for (auto& info : ctx.bind_groups) {
    if (info.entries.empty()) {
      continue;
    }

    auto layout = wgpuRenderPipelineGetBindGroupLayout(
        pipeline->GetRenderPipeline(), info.group);

    if (layout == nullptr) {
      continue;
    }

    auto bind_group = info.CreateBindGroup(device_, layout);

    if (bind_group == nullptr) {
      continue;
    }

    wgpuRenderPassEncoderSetBindGroup(render_pass, info.group, bind_group, 0,
                                      nullptr);

    command_buffer_->RecordBindGroup(bind_group);
  }
}

}  // namespace skity
