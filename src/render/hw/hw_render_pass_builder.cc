// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_render_pass_builder.hpp"

#include "src/gpu/gpu_context_impl.hpp"
#include "src/render/hw/hw_draw.hpp"

namespace skity {

void HWRenderPassBuilder::Build(GPURenderPassDescriptor &desc) {
  BuildColorAttachment(desc);

  BuildDepthStencilAttachment(desc);
}

void HWRenderPassBuilder::BuildColorAttachment(GPURenderPassDescriptor &desc) {
  desc.color_attachment.clear_value = {};
  desc.color_attachment.load_op = load_op_;
  desc.color_attachment.store_op = store_op_;

  if (sample_count_ == 1) {
    desc.color_attachment.texture = target_;
  } else {
    auto cache = ctx_->gpuContext->GetRenderTargetCache();

    GPUTextureDescriptor texture_desc;
    texture_desc.width = target_->GetDescriptor().width;
    texture_desc.height = target_->GetDescriptor().height;
    texture_desc.format = target_->GetDescriptor().format;
    texture_desc.storage_mode = GPUTextureStorageMode::kMemoryless;
    texture_desc.usage =
        static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);
    texture_desc.sample_count = sample_count_;

    auto color_attachment =
        cache->ObtainResource(texture_desc, ctx_->pool)->GetValue();

    desc.color_attachment.texture = color_attachment;
    desc.color_attachment.resolve_texture = target_;
  }
}

void HWRenderPassBuilder::BuildDepthStencilAttachment(
    GPURenderPassDescriptor &desc) {
  // Always set the depth and stencil load action to clear. This will force the
  // GL backend to clear the depth and stencil buffers when clearing.
  //
  // The reason for this is that there is a crash when calling 'glClear'. An MTK
  // engineer said that this is because the depth buffer is not cleared at the
  // beginning of the frame, although we have not found out why this happens.
  //
  // According to the 'glClear' API documentation: "If a buffer is not present,
  // then a glClear directed at that buffer has no effect.", so there is no risk
  // in doing this.
  desc.stencil_attachment.load_op = GPULoadOp::kClear;
  desc.stencil_attachment.store_op = GPUStoreOp::kDiscard;
  desc.stencil_attachment.clear_value = 0;

  desc.depth_attachment.load_op = GPULoadOp::kClear;
  desc.depth_attachment.store_op = GPUStoreOp::kDiscard;
  desc.depth_attachment.clear_value = 0.f;

  if (state_ == kDrawStateNone) {
    return;
  }

  auto format = GPUTextureFormat::kInvalid;

  if (state_ == (kDrawStateStencil | kDrawStateDepth)) {
    format = GPUTextureFormat::kDepth24Stencil8;
  } else {
    format = GPUTextureFormat::kStencil8;
  }

  auto cache = ctx_->gpuContext->GetRenderTargetCache();

  GPUTextureDescriptor texture_desc;
  texture_desc.width = target_->GetDescriptor().width;
  texture_desc.height = target_->GetDescriptor().height;
  texture_desc.format = format;
  texture_desc.storage_mode = GPUTextureStorageMode::kMemoryless;
  texture_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);
  texture_desc.sample_count = sample_count_;

  auto attachment = cache->ObtainResource(texture_desc, ctx_->pool)->GetValue();

  if (state_ & kDrawStateStencil) {
    desc.stencil_attachment.texture = attachment;
  }

  if (state_ & kDrawStateDepth) {
    desc.depth_attachment.texture = attachment;
  }
}

}  // namespace skity
