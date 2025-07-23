// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_SCALER_CONTEXT_DESC_HPP
#define SRC_TEXT_SCALER_CONTEXT_DESC_HPP

#include <cstdint>
#include <skity/graphic/paint.hpp>
#include <skity/text/font.hpp>
#include <skity/text/typeface.hpp>

#include "src/render/text/text_transform.hpp"

namespace skity {

// The underlying port accepts what kind of scale ratio.
enum class PortScaleType { kFull, kVertical };

// Make sure the objects has no padding.
struct ScalerContextDesc {
  // hash start
  uint32_t typeface_id;
  float text_size;
  float scale_x;
  float skew_x;
  Matrix22 transform;

  // scale ratio applied to surface
  float context_scale = 1.0f;

  float stroke_width;
  float miter_limit;
  Paint::Cap cap;
  Paint::Join join;

  uint8_t fake_bold;
  const uint8_t reserved_align1{0};
  // hash end

  friend inline bool operator==(const ScalerContextDesc& lhs,
                                const ScalerContextDesc& rhs) {
    return lhs.typeface_id == rhs.typeface_id &&
           lhs.text_size == rhs.text_size && lhs.scale_x == rhs.scale_x &&
           lhs.skew_x == rhs.skew_x && lhs.transform == rhs.transform &&
           lhs.stroke_width == rhs.stroke_width &&
           lhs.miter_limit == rhs.miter_limit &&
           lhs.context_scale == rhs.context_scale && lhs.cap == rhs.cap &&
           lhs.join == rhs.join;
  }

  friend inline bool operator!=(const ScalerContextDesc& lhs,
                                const ScalerContextDesc& rhs) {
    return !(lhs == rhs);
  }

  size_t hash() const;

  static ScalerContextDesc MakeCanonicalized(const Font& font,
                                             const Paint& paint);

  static ScalerContextDesc MakeTransformed(const Font& font, const Paint& paint,
                                           float context_scale,
                                           const Matrix22& transform);

  Matrix22 GetTransformMatrix() const;
  Matrix22 GetLocalMatrix() const;
  void DecomposeMatrix(PortScaleType type, float* scale_x, float* scale_y,
                       Matrix22* transform) const;
};

}  // namespace skity

#endif  // SRC_TEXT_SCALER_CONTEXT_DESC_HPP
