// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_STEP_COLOR_STEP_HPP
#define SRC_RENDER_HW_DRAW_STEP_COLOR_STEP_HPP

#include <skity/graphic/path.hpp>

#include "src/render/hw/draw/hw_draw_step.hpp"

namespace skity {

enum class CoverageType {
  kNone,
  kNoZero,
  kEvenOdd,
  kWinding,
};

class ColorStep : public HWDrawStep {
 public:
  ColorStep(HWWGSLGeometry* geometry, HWWGSLFragment* fragment,
            CoverageType coverage);

  ~ColorStep() override = default;

 protected:
  GPUStencilState GetStencilState() override;

  bool RequireDepthWrite() const override { return false; }

  bool RequireColorWrite() const override { return true; }

 private:
  CoverageType coverage_;
};

class ColorAAStep : public HWDrawStep {
 public:
  ColorAAStep(HWWGSLGeometry* geometry, HWWGSLFragment* fragment,
              CoverageType coverage);
  ~ColorAAStep() override = default;

 protected:
  GPUStencilState GetStencilState() override;

  bool RequireDepthWrite() const override { return false; }

  bool RequireColorWrite() const override { return true; }

 private:
  CoverageType coverage_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_STEP_COLOR_STEP_HPP
