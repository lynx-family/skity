// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_draw.hpp"

#include "src/tracing.hpp"

namespace skity {

HWDrawState HWDynamicDraw::OnPrepare(HWDrawContext* context) {
  SKITY_TRACE_EVENT(HWDynamicDraw_OnPrepare);

  OnGenerateDrawStep(steps_, context);

  HWDrawState state = HWDrawState::kDrawStateNone;

  for (auto& step : steps_) {
    if (step->RequireStencil()) {
      state |= HWDrawState::kDrawStateStencil;
    }

    if (step->RequireDepth()) {
      state |= HWDrawState::kDrawStateDepth;
    }
  }

  return state;
}

void HWDynamicDraw::OnGenerateCommand(HWDrawContext* context,
                                      HWDrawState state) {
  SKITY_TRACE_EVENT(HWDynamicDraw_OnGenerateCommand);

  HWDrawStepContext ctx{
      .context = context,
      .state = state,
      .transform = GetTransform(),
      .clip_depth = GetClipValue(),
      .scissor = GetScissorBox(),
      .color_format = GetColorFormat(),
      .sample_count = GetSampleCount(),
      .blend_mode = blend_mode_,
  };

  for (auto& step : steps_) {
    auto stencil_cmd = commands_.empty() ? nullptr : commands_.front();
    auto cmd = context->arena_allocator->Make<Command>();
    step->GenerateCommand(ctx, cmd, stencil_cmd);

    if (cmd->IsValid()) {
      commands_.push_back(cmd);
    }
  }
}

}  // namespace skity
