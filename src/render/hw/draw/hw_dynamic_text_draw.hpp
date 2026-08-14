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
  HWDynamicTextDraw(const Matrix& transform, HWWGSLGeometry* geometry,
                    HWWGSLFragment* fragment)
      : HWDynamicDraw(transform), geometry_(geometry), fragment_(fragment) {
    SetHasFragmentMask(fragment != nullptr && fragment->HasFragmentMask());
  }

  ~HWDynamicTextDraw() override = default;

  HWDrawType GetDrawType() const override { return HWDrawType::kText; }

  bool OnMergeIfPossible(HWDraw* draw) override;

  static Matrix CalcTransform(const Matrix& canvas_transform,
                              const Matrix& text_transform);

 protected:
  void OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                          HWDrawContext* context) override;

  HWWGSLGeometry* geometry_;
  HWWGSLFragment* fragment_;
};

class HWDynamicSdfTextDraw : public HWDynamicDraw {
 public:
  HWDynamicSdfTextDraw(const Matrix& transform, HWWGSLGeometry* geometry,
                       HWWGSLFragment* fragment)
      : HWDynamicDraw(transform), geometry_(geometry), fragment_(fragment) {
    SetHasFragmentMask(fragment != nullptr && fragment->HasFragmentMask());
  }

  ~HWDynamicSdfTextDraw() override = default;

  static Matrix CalcTransform(const Matrix& transform, const float scale);

 protected:
  void OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                          HWDrawContext* context) override;

  HWWGSLGeometry* geometry_;
  HWWGSLFragment* fragment_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_DYNAMIC_TEXT_DRAW_HPP
