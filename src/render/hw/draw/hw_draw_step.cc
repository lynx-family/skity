// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_draw_step.hpp"

#include "src/render/hw/hw_pipeline_lib.hpp"
#include "src/tracing.hpp"

namespace skity {

void HWDrawStep::GenerateCommand(const HWDrawStepContext& ctx, Command* cmd,
                                 Command* stencil_cmd) {
  SKITY_TRACE_EVENT(HWDrawStep_GenerateCommand);

  GPUScissorRect rect{};
  rect.x = static_cast<uint32_t>(std::floor(ctx.scissor.Left()));
  rect.y = static_cast<uint32_t>(std::floor(ctx.scissor.Top()));
  rect.width = static_cast<uint32_t>(std::ceil(ctx.scissor.Width()));
  rect.height = static_cast<uint32_t>(std::ceil(ctx.scissor.Height()));

  cmd->scissor_rect = rect;

  cmd->pipeline = GetPipeline(ctx.context, ctx.state, ctx.color_format,
                              ctx.sample_count, ctx.blend_mode);

  geometry_->PrepareCMD(cmd, ctx.context, ctx.transform, ctx.clip_depth,
                        stencil_cmd);

  fragment_->PrepareCMD(cmd, ctx.context);
}

GPURenderPipeline* HWDrawStep::GetPipeline(HWDrawContext* context,
                                           HWDrawState state,
                                           GPUTextureFormat target_format,
                                           uint32_t sample_count,
                                           BlendMode blend_mode) {
  SKITY_TRACE_EVENT(HWDrawStep_GetPipeline);

  HWPipelineDescriptor pipeline{};

  if (RequireColorWrite()) {
    pipeline.color_mask = 0xF;
    pipeline.blend_mode = blend_mode;
  } else {
    pipeline.color_mask = 0x0;
    pipeline.blend_mode = BlendMode::kDefault;
  }

  pipeline.color_format = target_format;
  pipeline.sample_count = sample_count;
  pipeline.buffers = geometry_->GetBufferLayout();

  if (state == HWDrawState::kDrawStateNone) {
    pipeline.depth_stencil.format = GPUTextureFormat::kInvalid;
    pipeline.depth_stencil.enable_stencil = false;
    pipeline.depth_stencil.enable_depth = false;
  } else {
    if (state & HWDrawState::kDrawStateDepth) {
      pipeline.depth_stencil.format = GPUTextureFormat::kDepth24Stencil8;
    } else {
      pipeline.depth_stencil.format = GPUTextureFormat::kStencil8;
    }

    if (RequireStencil()) {
      pipeline.depth_stencil.enable_stencil = true;
    }

    if (state & HWDrawState::kDrawStateDepth) {
      pipeline.depth_stencil.enable_depth = true;
      pipeline.depth_stencil.depth_state.enableWrite = RequireDepthWrite();
      pipeline.depth_stencil.depth_state.compare = GPUCompareFunction::kGreater;
    }

    pipeline.depth_stencil.stencil_state = GetStencilState();
  }

  pipeline.shader_generator = this;

  return context->pipelineLib->GetPipeline(
      {
          geometry_->GetShaderName(),
          fragment_->GetShaderName(),
      },
      pipeline);
}

}  // namespace skity
