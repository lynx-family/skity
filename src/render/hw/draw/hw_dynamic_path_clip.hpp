// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_DYNAMIC_PATH_CLIP_HPP
#define SRC_RENDER_HW_DRAW_HW_DYNAMIC_PATH_CLIP_HPP

#include <skity/graphic/path.hpp>
#include <skity/render/canvas.hpp>

#include "src/render/hw/draw/hw_dynamic_draw.hpp"

namespace skity {

class HWDynamicPathClip : public HWDynamicDraw {
 public:
  HWDynamicPathClip(Matrix transform, Path path, Canvas::ClipOp op,
                    const Rect &bounds);

  ~HWDynamicPathClip() override = default;

 protected:
  void OnGenerateDrawStep(ArrayList<HWDrawStep *, 2> &steps,
                          HWDrawContext *context) override;

 private:
  Path path_;
  Canvas::ClipOp op_;
  Path bounds_;
  Paint paint_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_DYNAMIC_PATH_CLIP_HPP
