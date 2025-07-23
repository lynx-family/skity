// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/text/typeface.hpp>

#include "src/geometry/math.hpp"

namespace skity {

Paint::Paint() = default;

Paint::~Paint() = default;

Paint& Paint::operator=(const Paint& paint) = default;

void Paint::Reset() { *this = Paint(); }

Paint::Style Paint::GetStyle() const { return style_; }

void Paint::SetStyle(Style style) {
  if (style > StyleCount) {
    return;
  }

  style_ = style;
}

void Paint::SetStrokeWidth(float width) { stroke_width_ = width; }

float Paint::GetStrokeWidth() const { return stroke_width_; }

float Paint::GetStrokeMiter() const { return miter_limit_; }

void Paint::SetStrokeMiter(float miter) { miter_limit_ = miter; }

Paint::Cap Paint::GetStrokeCap() const { return cap_; }

void Paint::SetStrokeCap(Cap cap) { cap_ = cap; }

Paint::Join Paint::GetStrokeJoin() const { return join_; }

void Paint::SetStrokeJoin(Join join) { join_ = join; }

void Paint::SetStrokeColor(float r, float g, float b, float a) {
  stroke_color_[0] = r;
  stroke_color_[1] = g;
  stroke_color_[2] = b;
  stroke_color_[3] = a;
}

void Paint::SetStrokeColor(const Vector& color) {
  SetStrokeColor(color.x, color.y, color.z, color.w);
}

void Paint::SetStrokeColor(Color color) {
  SetStrokeColor(Color4fFromColor(color));
}

Vector Paint::GetStrokeColor() const { return stroke_color_; }

void Paint::SetFillColor(float r, float g, float b, float a) {
  fill_color_[0] = r;
  fill_color_[1] = g;
  fill_color_[2] = b;
  fill_color_[3] = a;
}

void Paint::SetFillColor(const Vector& color) {
  SetFillColor(color.x, color.y, color.z, color.w);
}

void Paint::SetFillColor(Color color) { SetFillColor(Color4fFromColor(color)); }

Vector Paint::GetFillColor() const { return fill_color_; }

void Paint::SetColor(Color color) {
  auto color4f = Color4fFromColor(color);
  stroke_color_ = color4f;
  fill_color_ = color4f;
}

Color Paint::GetColor() const { return Color4fToColor(GetColor4f()); }

Color4f Paint::GetColor4f() const {
  return kStroke_Style == style_ ? stroke_color_ : fill_color_;
}

void Paint::SetAntiAlias(bool aa) { is_anti_alias_ = aa; }

bool Paint::IsAntiAlias() const { return is_anti_alias_; }

float Paint::GetAlphaF() const {
  return (kStroke_Style == style_) ? stroke_color_.a : fill_color_.a;
}

void Paint::SetAlphaF(float a) {
  a = glm::clamp(a, 0.f, 1.f);
  stroke_color_.a = a;
  fill_color_.a = a;
}

uint8_t Paint::GetAlpha() const {
  return static_cast<uint8_t>(std::round(this->GetAlphaF() * 255));
}

void Paint::SetAlpha(uint8_t alpha) { this->SetAlphaF(alpha * (1.f / 255)); }

Rect Paint::ComputeFastBounds(const Rect& origin) const {
  float radius = 0;

  if (GetStyle() != Paint::kFill_Style) {
    float multiplier = 1;
    if (Paint::kMiter_Join == join_) {
      multiplier = std::max(multiplier, miter_limit_);
    }
    if (Paint::kSquare_Cap == cap_) {
      multiplier = std::max(multiplier, FloatSqrt2);
    }
    radius = stroke_width_ / 2 * multiplier;
  }

  if (GetMaskFilter() != nullptr) {
    radius += GetMaskFilter()->GetBlurRadius();
  }

  Rect rect = Rect::MakeLTRB(origin.Left() - radius,   //
                             origin.Top() - radius,    //
                             origin.Right() + radius,  //
                             origin.Bottom() + radius);

  if (GetImageFilter() != nullptr) {
    rect = GetImageFilter()->ComputeFastBounds(rect);
  }

  return rect;
}

void Paint::SetAdjustStroke(bool adjust) { is_adjust_stroke_ = adjust; }

bool Paint::IsAdjustStroke() const { return is_adjust_stroke_; }

bool Paint::operator==(const Paint& other) const {
  return cap_ == other.cap_ &&                                  //
         join_ == other.join_ &&                                //
         style_ == other.style_ &&                              //
         stroke_width_ == other.stroke_width_ &&                //
         miter_limit_ == other.miter_limit_ &&                  //
         text_size_ == other.text_size_ &&                      //
         sdf_for_small_text_ == other.sdf_for_small_text_ &&    //
         font_fill_threshold_ == other.font_fill_threshold_ &&  //
         fill_color_ == other.fill_color_ &&                    //
         stroke_color_ == other.stroke_color_ &&                //
         is_anti_alias_ == other.is_anti_alias_ &&              //
         blend_mode_ == other.blend_mode_ &&                    //
         path_effect_ == other.path_effect_ &&                  //
         shader_ == other.shader_ &&                            //
         typeface_ == other.typeface_ &&                        //
         color_filter_ == other.color_filter_ &&                //
         image_filter_ == other.image_filter_ &&                //
         mask_filter_ == other.mask_filter_ &&                  //
         is_adjust_stroke_ == other.is_adjust_stroke_;
}

}  // namespace skity
