// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_STEP_CLIP_STEP_HPP
#define SRC_RENDER_HW_DRAW_STEP_CLIP_STEP_HPP

#include <skity/graphic/path.hpp>
#include <skity/render/canvas.hpp>

#include "src/render/hw/draw/hw_draw_step.hpp"

namespace skity {

class ClipStep : public HWDrawStep {
 public:
  ClipStep(HWWGSLGeometry* geometry, HWWGSLFragment* fragment,
           Path::PathFillType fill_type, Canvas::ClipOp op);
  ~ClipStep() override = default;

 protected:
  bool RequireDepthWrite() const override { return true; }

  bool RequireColorWrite() const override { return false; }

  GPUStencilState GetStencilState() override;

 private:
  Path::PathFillType fill_type_;
  Canvas::ClipOp op_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_STEP_CLIP_STEP_HPP
