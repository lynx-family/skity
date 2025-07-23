// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_LAYER_STATE_HPP
#define SRC_RENDER_HW_HW_LAYER_STATE_HPP

#include <functional>
#include <skity/graphic/blend_mode.hpp>
#include <skity/graphic/path.hpp>
#include <vector>

#include "src/render/hw/hw_draw.hpp"

namespace skity {

class HWLayerState {
 public:
  struct ClipStackValue {
    std::vector<HWDraw*> clip_draws;
    // clip bounds in physical pixel size in this layer
    // only applied with intersect clip
    Rect clip_bounds = {};
  };

  explicit HWLayerState(int32_t depth);

  ~HWLayerState() = default;

  void Save();
  void Restore();
  void RestoreToCount(int save_count);

  void SaveClipOp(HWDraw* clip);

  void SaveClipBounds(const Rect& bounds, bool reset = false);

  const Rect& CurrentClipBounds() const;

  HWDraw* LastClipDraw() const;

  int32_t GetCurrentDepth() const {
    return start_depth + clip_stack_.size() - 1;
  }

  int32_t GetSelfDepth() const { return clip_stack_.size(); }

  int32_t GetStartDepth() const { return start_depth; }

  uint32_t GetNextDrawDepth();

  uint32_t GetDrawDepth() const { return draw_depth_; }

  void FlushClipDepth();

 private:
  void PushClipStack();
  void PopClipStack();

 private:
  int32_t start_depth = 1;
  std::vector<ClipStackValue> clip_stack_ = {};
  uint32_t draw_depth_ = 0;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_LAYER_STATE_HPP
