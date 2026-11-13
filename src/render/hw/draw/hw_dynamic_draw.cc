// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_draw.hpp"

#include <skity/graphic/paint.hpp>

#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/tracing.hpp"

namespace skity {

bool HWDynamicDraw::CanMergeSolidPaints(const Paint& paint,
                                        const Paint& other) {
  return !paint.GetShader() && !other.GetShader() &&
         paint.GetColorFilter() == other.GetColorFilter();
}

HWDrawStep* HWDynamicDraw::MakeShapeColorStep(HWDrawContext* context,
                                              HWWGSLGeometry* geometry,
                                              const Paint& paint) const {
  auto* fragment = GenShadingFragment(context, paint,
                                      paint.GetStyle() == Paint::kStroke_Style,
                                      /*has_color=*/false);
  ConfigureShadingFragment(context, paint, GetBlendPlan(), fragment);
  return context->arena_allocator->Make<ColorStep>(geometry, fragment,
                                                   CoverageType::kNone);
}

HWDrawState HWDynamicDraw::OnPrepare(HWDrawContext* context) {
  SKITY_TRACE_EVENT(HWDynamicDraw_OnPrepare);

  steps_.SetArenaAllocator(context->arena_allocator);
  OnGenerateDrawStep(steps_, context);

  HWDrawState state = HWDrawState::kDrawStateNone;

  for (auto& step : steps_) {
    if (step->RequireStencil()) {
      state |= HWDrawState::kDrawStateStencil;
    }

    if (step->RequireDepth()) {
      // Depth attachments use a combined depth/stencil format.
      state |= HWDrawState::kDrawStateDepth | HWDrawState::kDrawStateStencil;
    }
  }

  return state;
}

void HWDynamicDraw::OnGenerateCommand(HWDrawContext* context,
                                      HWDrawState state) {
  SKITY_TRACE_EVENT(HWDynamicDraw_OnGenerateCommand);

  HWDrawStepContext ctx{
      context,          state,           GetTransform(),
      GetClipValue(),   GetScissorBox(), GetColorFormat(),
      GetSampleCount(), GetBlendPlan(),  context->scale,
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
