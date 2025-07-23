// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_layer_state.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace skity {

HWLayerState::HWLayerState(int32_t depth) : start_depth(depth) {
  clip_stack_.emplace_back();
}

void HWLayerState::Save() { PushClipStack(); }

void HWLayerState::Restore() { PopClipStack(); }

void HWLayerState::RestoreToCount(int save_count) {
  while (static_cast<size_t>(save_count) < clip_stack_.size()) {
    PopClipStack();
  }
}

void HWLayerState::SaveClipOp(skity::HWDraw *clip) {
  clip_stack_.back().clip_draws.emplace_back(clip);
}

void HWLayerState::SaveClipBounds(const Rect &bounds, bool reset) {
  if (reset) {
    clip_stack_.back().clip_bounds = bounds;
  } else {
    if (!clip_stack_.back().clip_bounds.Intersect(bounds)) {
      clip_stack_.back().clip_bounds.SetEmpty();
    }
  }
}

const Rect &HWLayerState::CurrentClipBounds() const {
  return clip_stack_.back().clip_bounds;
}

HWDraw *HWLayerState::LastClipDraw() const {
  for (auto it = clip_stack_.rbegin(); it != clip_stack_.rend(); it++) {
    if (!it->clip_draws.empty()) {
      return it->clip_draws.back();
    }
  }

  return nullptr;
}

uint32_t HWLayerState::GetNextDrawDepth() { return ++draw_depth_; }

void HWLayerState::FlushClipDepth() {
  for (auto it = clip_stack_.rbegin(); it != clip_stack_.rend(); it++) {
    for (auto draw_it = it->clip_draws.rbegin();
         draw_it != it->clip_draws.rend(); draw_it++) {
      (*draw_it)->SetClipDepth(GetNextDrawDepth());
    }
  }
}

void HWLayerState::PushClipStack() {
  auto bounds = clip_stack_.back().clip_bounds;
  clip_stack_.emplace_back();
  clip_stack_.back().clip_bounds = bounds;
}

void HWLayerState::PopClipStack() {
  auto &clip_draws = clip_stack_.back().clip_draws;

  for (auto it = clip_draws.rbegin(); it != clip_draws.rend(); it++) {
    (*it)->SetClipDepth(GetNextDrawDepth());
  }

  clip_stack_.pop_back();
}

}  // namespace skity
