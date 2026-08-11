// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_font.h>

#include <skity/text/font.hpp>
#include <skity/text/font_metrics.hpp>
#include <skity/text/typeface.hpp>

#include "handle.hpp"

namespace {

skity::Font* font_of(skity_font handle) {
  auto* w = skity::capi::resolve<skity_font_s>(handle, SKITY_OBJECT_TYPE_FONT);
  return w ? static_cast<skity::Font*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

skity_font skity_font_create(void) {
  return skity::capi::alloc_handle<skity_font_s>(
      SKITY_OBJECT_TYPE_FONT, SKITY_HANDLE_OWNING,
      std::make_shared<skity::Font>());
}

skity_font skity_font_create_with_typeface(skity_typeface typeface,
                                           float size) {
  auto tf = skity::capi::get_impl<skity_typeface_s, skity::Typeface>(
      typeface, SKITY_OBJECT_TYPE_TYPEFACE);
  if (tf == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_font_s>(
      SKITY_OBJECT_TYPE_FONT, SKITY_HANDLE_OWNING,
      std::make_shared<skity::Font>(tf, size));
}

skity_font skity_font_create_with_typeface_scale(skity_typeface typeface,
                                                 float size, float scale_x,
                                                 float skew_x) {
  auto tf = skity::capi::get_impl<skity_typeface_s, skity::Typeface>(
      typeface, SKITY_OBJECT_TYPE_TYPEFACE);
  if (tf == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_font_s>(
      SKITY_OBJECT_TYPE_FONT, SKITY_HANDLE_OWNING,
      std::make_shared<skity::Font>(tf, size, scale_x, skew_x));
}

void skity_font_destroy(skity_font font) {
  skity::capi::destroy_handle<skity_font_s>(font, SKITY_OBJECT_TYPE_FONT);
}

void skity_font_set_typeface(skity_font font, skity_typeface typeface) {
  auto* f = font_of(font);
  auto tf = skity::capi::get_impl<skity_typeface_s, skity::Typeface>(
      typeface, SKITY_OBJECT_TYPE_TYPEFACE);
  if (f != nullptr && tf != nullptr) {
    f->SetTypeface(tf);
  }
}

skity_typeface skity_font_get_typeface(skity_font font) {
  auto* f = font_of(font);
  if (f == nullptr) return nullptr;
  auto sp = f->GetTypeface();
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(sp));
}

void skity_font_set_size(skity_font font, float size) {
  if (auto* f = font_of(font)) f->SetSize(size);
}

float skity_font_get_size(skity_font font) {
  auto* f = font_of(font);
  return f ? f->GetSize() : 0.0f;
}

float skity_font_get_scale_x(skity_font font) {
  auto* f = font_of(font);
  return f ? f->GetScaleX() : 1.0f;
}

void skity_font_set_scale_x(skity_font font, float scale_x) {
  if (auto* f = font_of(font)) f->SetScaleX(scale_x);
}

float skity_font_get_skew_x(skity_font font) {
  auto* f = font_of(font);
  return f ? f->GetSkewX() : 0.0f;
}

void skity_font_set_skew_x(skity_font font, float skew_x) {
  if (auto* f = font_of(font)) f->SetSkewX(skew_x);
}

void skity_font_get_metrics(skity_font font, skity_font_metrics* out) {
  auto* f = font_of(font);
  if (f == nullptr || out == nullptr) {
    return;
  }
  skity::FontMetrics m;
  f->GetMetrics(&m);
  out->top = m.top_;
  out->ascent = m.ascent_;
  out->descent = m.descent_;
  out->bottom = m.bottom_;
  out->leading = m.leading_;
  out->avg_char_width = m.avg_char_width_;
  out->max_char_width = m.max_char_width_;
  out->x_min = m.x_min_;
  out->x_max = m.x_max_;
  out->x_height = m.x_height_;
  out->cap_height = m.cap_height_;
  out->underline_thickness = m.underline_thickness_;
  out->underline_position = m.underline_position_;
  out->strikeout_thickness = m.strikeout_thickness_;
  out->strikeout_position = m.strikeout_position_;
}

void skity_font_set_hinting(skity_font font, skity_font_hinting hinting) {
  if (auto* f = font_of(font))
    f->SetHinting(static_cast<skity::Font::FontHinting>(hinting));
}

skity_font_hinting skity_font_get_hinting(skity_font font) {
  auto* f = font_of(font);
  return f ? static_cast<skity_font_hinting>(f->GetHinting())
           : SKITY_FONT_HINTING_NORMAL;
}

void skity_font_set_edging(skity_font font, skity_font_edging edging) {
  if (auto* f = font_of(font))
    f->SetEdging(static_cast<skity::Font::Edging>(edging));
}

skity_font_edging skity_font_get_edging(skity_font font) {
  auto* f = font_of(font);
  return f ? static_cast<skity_font_edging>(f->GetEdging())
           : SKITY_FONT_EDGING_ALIAS;
}

void skity_font_set_subpixel(skity_font font, uint32_t enable) {
  if (auto* f = font_of(font)) f->SetSubpixel(enable != 0);
}

void skity_font_set_force_auto_hinting(skity_font font, uint32_t enable) {
  if (auto* f = font_of(font)) f->SetForceAutoHinting(enable != 0);
}

void skity_font_set_embedded_bitmaps(skity_font font, uint32_t enable) {
  if (auto* f = font_of(font)) f->SetEmbeddedBitmaps(enable != 0);
}

void skity_font_set_linear_metrics(skity_font font, uint32_t enable) {
  if (auto* f = font_of(font)) f->SetLinearMetrics(enable != 0);
}

void skity_font_set_embolden(skity_font font, uint32_t enable) {
  if (auto* f = font_of(font)) f->SetEmbolden(enable != 0);
}

void skity_font_set_baseline_snap(skity_font font, uint32_t enable) {
  if (auto* f = font_of(font)) f->SetBaselineSnap(enable != 0);
}

uint32_t skity_font_is_force_auto_hinting(skity_font font) {
  auto* f = font_of(font);
  return f && f->IsForceAutoHinting() ? 1u : 0u;
}

uint32_t skity_font_is_embedded_bitmaps(skity_font font) {
  auto* f = font_of(font);
  return f && f->IsEmbeddedBitmaps() ? 1u : 0u;
}

uint32_t skity_font_is_subpixel(skity_font font) {
  auto* f = font_of(font);
  return f && f->IsSubpixel() ? 1u : 0u;
}

uint32_t skity_font_is_linear_metrics(skity_font font) {
  auto* f = font_of(font);
  return f && f->IsLinearMetrics() ? 1u : 0u;
}

uint32_t skity_font_is_embolden(skity_font font) {
  auto* f = font_of(font);
  return f && f->IsEmbolden() ? 1u : 0u;
}

uint32_t skity_font_is_baseline_snap(skity_font font) {
  auto* f = font_of(font);
  return f && f->IsBaselineSnap() ? 1u : 0u;
}

skity_font skity_font_make_with_size(skity_font font, float size) {
  auto* f = font_of(font);
  if (f == nullptr) return nullptr;
  auto nf = std::make_shared<skity::Font>(f->MakeWithSize(size));
  return skity::capi::alloc_handle<skity_font_s>(
      SKITY_OBJECT_TYPE_FONT, SKITY_HANDLE_OWNING, std::move(nf));
}

void skity_font_get_widths(skity_font font, const uint16_t* glyphs,
                           int32_t count, float* widths) {
  auto* f = font_of(font);
  if (f == nullptr || glyphs == nullptr || widths == nullptr || count <= 0) {
    return;
  }
  f->GetWidths(reinterpret_cast<const skity::GlyphID*>(glyphs), count, widths);
}

}  // extern "C"
