// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_DYNAMIC_RECT_DRAW_HPP
#define SRC_RENDER_HW_DRAW_HW_DYNAMIC_RECT_DRAW_HPP

#include <skity/geometry/rect.hpp>
#include <skity/graphic/paint.hpp>
#include <vector>

#include "src/render/hw/draw/geometry/wgsl_rect_geometry.hpp"
#include "src/render/hw/draw/hw_dynamic_draw.hpp"

namespace skity {

class HWDynamicRectDraw : public HWDynamicDraw {
 public:
  HWDynamicRectDraw(const Matrix& transform, const Rect& rect,
                    const Paint& paint);

  HWDrawType GetDrawType() const override { return HWDrawType::kRect; }

 protected:
  bool OnMergeIfPossible(HWDraw* draw) override;
  void OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                          HWDrawContext* context) override;

 private:
  Paint paint_;
  WGSLRectGeometry::Instance first_instance_;
  std::vector<WGSLRectGeometry::Instance> additional_instances_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_DYNAMIC_RECT_DRAW_HPP
