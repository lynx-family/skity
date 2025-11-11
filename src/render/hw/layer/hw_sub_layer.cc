// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/layer/hw_sub_layer.hpp"

#include "src/gpu/gpu_context_impl.hpp"
#include "src/render/hw/draw/fragment/wgsl_texture_fragment.hpp"
#include "src/render/hw/draw/hw_dynamic_path_draw.hpp"
#include "src/render/hw/hw_render_pass_builder.hpp"

namespace skity {

HWDrawState HWSubLayer::OnPrepare(HWDrawContext* context) {
  InitTexture(context->gpuContext, context->pool);

  // prepare layer back draw
  {
    auto bounds = GetLayerBackDrawBounds();
    Path path;
    path.AddRect(bounds);

    Paint paint;
    paint.SetBlendMode(blend_mode_);
    paint.SetAlphaF(alpha_);
    paint.SetStyle(Paint::kFill_Style);
    paint.SetShader(CreateDrawLayerShader(context->gpuContext,
                                          layer_back_draw_texture_, bounds));

    layer_back_draw_ = context->arena_allocator->Make<HWDynamicPathDraw>(
        GetTransform(), std::move(path), std::move(paint), false, false);
  }

  layer_back_draw_->SetClipDepth(GetClipDepth());
  layer_back_draw_->SetSampleCount(GetSampleCount());
  layer_back_draw_->SetColorFormat(GetColorFormat());
  layer_back_draw_->SetScissorBox(GetScissorBox());

  auto state = layer_back_draw_->Prepare(context);

  // need to prepare self before all children
  // so can make sure self layer texture is not used by children layer
  HWLayer::OnPrepare(context);

  PrepareRenderPassDesc(context);

  return state;
}

void HWSubLayer::OnGenerateCommand(HWDrawContext* context, HWDrawState state) {
  HWLayer::OnGenerateCommand(context, state);

  layer_back_draw_->GenerateCommand(context, state);
}

std::shared_ptr<GPURenderPass> HWSubLayer::OnBeginRenderPass(
    GPUCommandBuffer* cmd) {
  return cmd->BeginRenderPass(render_pass_desc_);
}

void HWSubLayer::OnPostDraw(GPURenderPass* render_pass, GPUCommandBuffer* cmd) {
  layer_back_draw_->Draw(render_pass);
}

void HWSubLayer::InitTexture(GPUContextImpl* gpu_context,
                             HWRenderTargetCache::Pool* pool) {
  if (color_texture_ && layer_back_draw_texture_) {
    return;
  }

  auto render_target = gpu_context->GetRenderTargetCache()->ObtainResource(
      GetColorTextureDesc(), pool);
  color_texture_ = render_target->GetValue();
  layer_back_draw_texture_ = color_texture_;
}

void HWSubLayer::PrepareRenderPassDesc(HWDrawContext* context) {
  HWRenderPassBuilder builder(context, color_texture_);

  builder.SetSampleCount(GetSampleCount())
      .SetDrawState(GetLayerDrawState())
      .Build(render_pass_desc_);
}

}  // namespace skity
