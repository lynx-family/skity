// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_TEXT_RENDER_CONTROL_HPP
#define SRC_RENDER_TEXT_TEXT_RENDER_CONTROL_HPP

#include <memory>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/paint.hpp>
#include <vector>

namespace skity {

static constexpr int kSmallDFFontSize = 32;
static constexpr int kMediumDFFontSize = 72;
static constexpr int kLargeDFFontSize = 162;

static constexpr int kDefaultMinDistanceFieldFontSize = 18;
#ifdef SKITY_ANDROID
static constexpr int kDefaultMaxDistanceFieldFontSize = 384;
#else
static constexpr int kDefaultMaxDistanceFieldFontSize = 2 * kLargeDFFontSize;
#endif

class TextRenderControl {
 public:
  TextRenderControl(bool disallow_sdf = false,
                    float min_sdf_size = kLargeDFFontSize,
                    float max_sdf_size = kDefaultMaxDistanceFieldFontSize);

  bool CanUseSDF(float text_size, const Paint& paint, const Typeface* typeface);
  bool CanUseDirect(float text_size, const Matrix& transform,
                    const Paint& paint, const Typeface* typeface);

 private:
  bool disallow_sdf_;
  float min_sdf_size_;
  float max_sdf_size_;
};

}  // namespace skity

#endif  // SRC_RENDER_TEXT_TEXT_RENDER_CONTROL_HPP
