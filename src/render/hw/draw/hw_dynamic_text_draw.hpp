// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_DYNAMIC_TEXT_DRAW_HPP
#define SRC_RENDER_HW_DRAW_HW_DYNAMIC_TEXT_DRAW_HPP

#include "src/render/hw/draw/hw_dynamic_draw.hpp"
#include "src/render/hw/draw/hw_wgsl_fragment.hpp"
#include "src/render/hw/draw/hw_wgsl_geometry.hpp"

namespace skity {

class HWDynamicTextDraw : public HWDynamicDraw {
 public:
  HWDynamicTextDraw(const Matrix& canvas_transform,
                    const Matrix& text_transform, BlendMode blend_mode,
                    HWWGSLGeometry* geometry, HWWGSLFragment* fragment)
      : HWDynamicDraw(CalcTransform(canvas_transform, text_transform),
                      blend_mode),
        geometry_(geometry),
        fragment_(fragment) {}

  ~HWDynamicTextDraw() override = default;

 protected:
  void OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                          HWDrawContext* context) override;

 private:
  Matrix CalcTransform(const Matrix& canvas_transform,
                       const Matrix& text_transform) const;

  HWWGSLGeometry* geometry_;
  HWWGSLFragment* fragment_;
};

class HWDynamicSdfTextDraw : public HWDynamicDraw {
 public:
  HWDynamicSdfTextDraw(const Matrix& transform, const float scale,
                       BlendMode blend_mode, HWWGSLGeometry* geometry,
                       HWWGSLFragment* fragment)
      : HWDynamicDraw(CalcTransform(transform, scale), blend_mode),
        geometry_(geometry),
        fragment_(fragment) {}

  ~HWDynamicSdfTextDraw() override = default;

 protected:
  void OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                          HWDrawContext* context) override;

 private:
  Matrix CalcTransform(const Matrix& transform, const float scale) const;

  HWWGSLGeometry* geometry_;
  HWWGSLFragment* fragment_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_DYNAMIC_TEXT_DRAW_HPP
