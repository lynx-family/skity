// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_EFFECT_MASK_FILTER_PRIV_HPP
#define SRC_EFFECT_MASK_FILTER_PRIV_HPP

namespace skity {

class Bitmap;
class Canvas;
class MaskFilter;
class Paint;
class Rect;

#if defined(SKITY_CPU)
void MaskFilterOnFilter(Canvas* canvas, Bitmap& bitmap,
                        const Rect& filter_bounds, const Paint& paint,
                        MaskFilter* mask_filter);
#endif

}  // namespace skity

#endif  // SRC_EFFECT_MASK_FILTER_PRIV_HPP
