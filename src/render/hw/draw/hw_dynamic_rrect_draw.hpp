// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_DYNAMIC_RRECT_DRAW_HPP
#define SRC_RENDER_HW_DRAW_HW_DYNAMIC_RRECT_DRAW_HPP

#include <skity/geometry/rrect.hpp>
#include <skity/graphic/paint.hpp>
#include <vector>

#include "src/render/hw/draw/hw_dynamic_draw.hpp"
#include "src/utils/batch_group.hpp"

namespace skity {

class HWDynamicRRectDraw : public HWDynamicDraw {
 public:
  HWDynamicRRectDraw(const Matrix& transform, const RRect& rrect,
                     const Paint& paint);

  HWDrawType GetDrawType() const override { return HWDrawType::kRRect; }

 protected:
  bool OnMergeIfPossible(HWDraw* draw) override;
  void OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                          HWDrawContext* context) override;

 private:
  std::vector<BatchGroup<RRect>> batch_group_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_DYNAMIC_RRECT_DRAW_HPP
