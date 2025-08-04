// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/text_render_control.hpp"

#include <skity/text/typeface.hpp>

namespace skity {

TextRenderControl::TextRenderControl(bool disallow_sdf, float min_sdf_size,
                                     float max_sdf_size)
    : disallow_sdf_(disallow_sdf),
      min_sdf_size_(min_sdf_size),
      max_sdf_size_(max_sdf_size) {}

bool TextRenderControl::CanUseSDF(float text_size, const Paint& paint,
                                  const std::shared_ptr<Typeface>& typeface) {
  float min_sdf_size = paint.IsSDFForSmallText()
                           ? kDefaultMinDistanceFieldFontSize
                           : min_sdf_size_;
  return !disallow_sdf_ && text_size >= min_sdf_size &&
         text_size <= max_sdf_size_ && !typeface->ContainsColorTable() &&
         text_size < paint.GetFontThreshold();
}

bool TextRenderControl::CanUseDirect(
    float text_size, const Matrix& transform, const Paint& paint,
    const std::shared_ptr<Typeface>& typeface) {
  return !CanUseSDF(text_size, paint, typeface) && !transform.HasPersp() &&
         text_size < paint.GetFontThreshold();
}

}  // namespace skity
