// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_draw.hpp"

#include "src/tracing.hpp"
#include "src/utils/arena_allocator.hpp"

namespace skity {

HWDrawState HWDraw::Prepare(HWDrawContext* context) {
  SKITY_TRACE_EVENT(HWDraw_Prepare);

  if (!prepared_) {
    if (clip_depth_ > 0) {
      clip_value_ = static_cast<float>(clip_depth_) /
                    static_cast<float>(context->total_clip_depth + 1);
    }

    draw_state_ = OnPrepare(context);

    prepared_ = true;
  }

  return draw_state_;
}

void HWDraw::GenerateCommand(HWDrawContext* context, HWDrawState state) {
  SKITY_TRACE_EVENT(HWDraw_GenerateCommand);

  if (generated_) {
    return;
  }

  OnGenerateCommand(context, state);

  generated_ = true;
}

}  // namespace skity
