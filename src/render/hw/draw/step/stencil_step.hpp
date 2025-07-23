// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_STEP_STENCIL_STEP_HPP
#define SRC_RENDER_HW_DRAW_STEP_STENCIL_STEP_HPP

#include "src/render/hw/draw/hw_draw_step.hpp"

namespace skity {

class StencilStep : public HWDrawStep {
 public:
  StencilStep(HWWGSLGeometry* geometry, HWWGSLFragment* fragment, bool no_zero);

  ~StencilStep() override = default;

 protected:
  GPUStencilState GetStencilState() override;

  bool RequireDepthWrite() const override { return false; }

  bool RequireColorWrite() const override { return false; }

 private:
  bool no_zero_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_STEP_STENCIL_STEP_HPP
