// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_PAINT_ORDER_HPP
#define SRC_RENDER_PAINT_ORDER_HPP

#include <skity/graphic/paint.hpp>

namespace skity {

template <typename FillFunc, typename StrokeFunc>
void DrawFillStrokeInPaintOrder(Paint::Style style, bool need_fill,
                                bool need_stroke, FillFunc&& draw_fill,
                                StrokeFunc&& draw_stroke) {
  if (style == Paint::kStrokeThenFill_Style) {
    if (need_stroke) {
      draw_stroke();
    }
    if (need_fill) {
      draw_fill();
    }
  } else {
    if (need_fill) {
      draw_fill();
    }
    if (need_stroke) {
      draw_stroke();
    }
  }
}

}  // namespace skity

#endif  // SRC_RENDER_PAINT_ORDER_HPP
