// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/scaler_context_desc.hpp"

#include <cmath>
#include <cstdlib>

#include "src/base/hash.hpp"
#include "src/geometry/math.hpp"
#include "src/text/scaler_context_cache.hpp"

namespace skity {

size_t ScalerContextDesc::hash() const {
  return skity::Hash32(this, sizeof(ScalerContextDesc));
}

ScalerContextDesc ScalerContextDesc::MakeCanonicalized(const Font& font,
                                                       const Paint& paint) {
  ScalerContextDesc desc;
  desc.typeface_id = font.GetTypeface()->TypefaceId();
  desc.text_size = font.GetSize();
  desc.scale_x = font.GetScaleX();
  desc.skew_x = font.GetSkewX();
  desc.transform = Matrix22{};
  desc.stroke_width =
      paint.GetStyle() == Paint::kStroke_Style ? paint.GetStrokeWidth() : 0.f;
  desc.miter_limit = paint.GetStrokeMiter();
  desc.cap = paint.GetStrokeCap();
  desc.join = paint.GetStrokeJoin();
  desc.fake_bold = font.IsEmbolden() ? 1 : 0;

  desc.context_scale = 1.0f;

  return desc;
}

ScalerContextDesc ScalerContextDesc::MakeTransformed(
    const Font& font, const Paint& paint, float context_scale,
    const Matrix22& transform) {
  ScalerContextDesc desc;
  desc.typeface_id = font.GetTypeface()->TypefaceId();
  desc.text_size = font.GetSize();
  desc.scale_x = font.GetScaleX();
  desc.skew_x = font.GetSkewX();
  desc.transform = transform;

  desc.stroke_width =
      paint.GetStyle() == Paint::kStroke_Style ? paint.GetStrokeWidth() : 0.f;
  desc.miter_limit = paint.GetStyle() == Paint::kStroke_Style
                         ? paint.GetStrokeMiter()
                         : Paint::kDefaultMiterLimit;
  desc.cap = paint.GetStyle() == Paint::kStroke_Style ? paint.GetStrokeCap()
                                                      : Paint::kDefault_Cap;
  desc.join = paint.GetStyle() == Paint::kStroke_Style ? paint.GetStrokeJoin()
                                                       : Paint::kDefault_Join;
  desc.fake_bold = font.IsEmbolden() ? 1 : 0;

  desc.context_scale = context_scale;

  return desc;
}

Matrix22 ScalerContextDesc::GetTransformMatrix() const { return transform; }

Matrix22 ScalerContextDesc::GetLocalMatrix() const {
  float text_scale_x = scale_x * text_size;
  float text_scale_y = text_size;
  // scale then skew
  return Matrix22{text_scale_x, skew_x == 0.f ? 0.f : skew_x * text_scale_y,
                  0.f, text_scale_y};
}

void ScalerContextDesc::DecomposeMatrix(PortScaleType type, float* scale_x,
                                        float* scale_y,
                                        Matrix22* transform) const {
  Matrix22 total = GetTransformMatrix() * GetLocalMatrix();

  bool only_scale =
      FloatNearlyZero(total.skew_x_) && FloatNearlyZero(total.skew_y_);
  if (only_scale) {
    if (FloatNearlyZero(total.scale_x_) || FloatNearlyZero(total.scale_y_)) {
      *scale_x = 0.f;
      *scale_y = 0.f;
      transform->scale_x_ = 1.f;
      transform->scale_y_ = 1.f;
      return;
    }
    if (type == PortScaleType::kVertical &&
        !FloatNearlyZero(total.scale_x_ - total.scale_y_)) {
      *scale_y = std::fabs(total.scale_y_);
      *scale_x = *scale_y;
      transform->scale_x_ = total.scale_x_ / *scale_x;
      transform->scale_y_ = total.scale_y_ < 0.f ? -1.f : 1.f;
    } else {
      *scale_x = std::fabs(total.scale_x_);
      *scale_y = std::fabs(total.scale_y_);
      transform->scale_x_ = total.scale_x_ < 0.f ? -1.f : 1.f;
      transform->scale_y_ = total.scale_y_ < 0.f ? -1.f : 1.f;
    }
  } else {
    Matrix22 q, r;
    total.QRDecompose(&q, &r);
    if (FloatNearlyZero(r.scale_x_) || FloatNearlyZero(r.scale_y_)) {
      *scale_x = 0.f;
      *scale_y = 0.f;
      transform->scale_x_ = 1.f;
      transform->scale_y_ = 1.f;
      return;
    }
    *scale_y = std::fabs(r.scale_y_);
    *scale_x = type == PortScaleType::kFull ? std::fabs(r.scale_x_) : *scale_y;
    *transform = total * Matrix22(1 / *scale_x, 0, 0, 1 / *scale_y);
  }
}

}  // namespace skity
