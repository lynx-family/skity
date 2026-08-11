// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_paint.h>

#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>
#include <skity/effect/path_effect.hpp>
#include <skity/effect/shader.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/text/typeface.hpp>

#include "handle.hpp"

namespace {

skity::Paint* paint_of(skity_paint handle) {
  auto* w =
      skity::capi::resolve<skity_paint_s>(handle, SKITY_OBJECT_TYPE_PAINT);
  return w ? static_cast<skity::Paint*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

skity_paint skity_paint_create(void) {
  auto impl = std::make_shared<skity::Paint>();
  return skity::capi::alloc_handle<skity_paint_s>(
      SKITY_OBJECT_TYPE_PAINT, SKITY_HANDLE_OWNING, std::move(impl));
}

void skity_paint_destroy(skity_paint paint) {
  skity::capi::destroy_handle<skity_paint_s>(paint, SKITY_OBJECT_TYPE_PAINT);
}

void skity_paint_reset(skity_paint paint) {
  if (auto* p = paint_of(paint)) p->Reset();
}

void skity_paint_set_style(skity_paint paint, skity_paint_style style) {
  if (auto* p = paint_of(paint))
    p->SetStyle(static_cast<skity::Paint::Style>(style));
}

void skity_paint_set_stroke_width(skity_paint paint, float width) {
  if (auto* p = paint_of(paint)) p->SetStrokeWidth(width);
}

void skity_paint_set_stroke_cap(skity_paint paint, skity_paint_cap cap) {
  if (auto* p = paint_of(paint))
    p->SetStrokeCap(static_cast<skity::Paint::Cap>(cap));
}

void skity_paint_set_stroke_join(skity_paint paint, skity_paint_join join) {
  if (auto* p = paint_of(paint))
    p->SetStrokeJoin(static_cast<skity::Paint::Join>(join));
}

void skity_paint_set_stroke_miter(skity_paint paint, float miter) {
  if (auto* p = paint_of(paint)) p->SetStrokeMiter(miter);
}

void skity_paint_set_color(skity_paint paint, skity_color color) {
  if (auto* p = paint_of(paint)) p->SetColor(static_cast<skity::Color>(color));
}

void skity_paint_set_stroke_color(skity_paint paint, skity_color color) {
  if (auto* p = paint_of(paint))
    p->SetStrokeColor(static_cast<skity::Color>(color));
}

void skity_paint_set_fill_color(skity_paint paint, skity_color color) {
  if (auto* p = paint_of(paint))
    p->SetFillColor(static_cast<skity::Color>(color));
}

void skity_paint_set_alpha(skity_paint paint, uint8_t alpha) {
  if (auto* p = paint_of(paint)) p->SetAlpha(alpha);
}

void skity_paint_set_alpha_f(skity_paint paint, float alpha) {
  if (auto* p = paint_of(paint)) p->SetAlphaF(alpha);
}

void skity_paint_set_blend_mode(skity_paint paint, skity_blend_mode mode) {
  if (auto* p = paint_of(paint))
    p->SetBlendMode(static_cast<skity::BlendMode>(mode));
}

void skity_paint_set_anti_alias(skity_paint paint, uint32_t aa) {
  if (auto* p = paint_of(paint)) p->SetAntiAlias(aa != 0);
}

void skity_paint_set_text_size(skity_paint paint, float size) {
  if (auto* p = paint_of(paint)) p->SetTextSize(size);
}

void skity_paint_set_shader(skity_paint paint, skity_shader shader) {
  auto* p = paint_of(paint);
  if (p == nullptr) return;
  if (shader != nullptr) {
    p->SetShader(skity::capi::get_impl<skity_shader_s, skity::Shader>(
        shader, SKITY_OBJECT_TYPE_SHADER));
  } else {
    p->SetShader(nullptr);
  }
}

void skity_paint_set_color_filter(skity_paint paint,
                                  skity_color_filter filter) {
  auto* p = paint_of(paint);
  if (p == nullptr) return;
  if (filter != nullptr) {
    p->SetColorFilter(
        skity::capi::get_impl<skity_color_filter_s, skity::ColorFilter>(
            filter, SKITY_OBJECT_TYPE_COLOR_FILTER));
  } else {
    p->SetColorFilter(nullptr);
  }
}

void skity_paint_set_image_filter(skity_paint paint,
                                  skity_image_filter filter) {
  auto* p = paint_of(paint);
  if (p == nullptr) return;
  if (filter != nullptr) {
    p->SetImageFilter(
        skity::capi::get_impl<skity_image_filter_s, skity::ImageFilter>(
            filter, SKITY_OBJECT_TYPE_IMAGE_FILTER));
  } else {
    p->SetImageFilter(nullptr);
  }
}

void skity_paint_set_mask_filter(skity_paint paint, skity_mask_filter filter) {
  auto* p = paint_of(paint);
  if (p == nullptr) return;
  if (filter != nullptr) {
    p->SetMaskFilter(
        skity::capi::get_impl<skity_mask_filter_s, skity::MaskFilter>(
            filter, SKITY_OBJECT_TYPE_MASK_FILTER));
  } else {
    p->SetMaskFilter(nullptr);
  }
}

void skity_paint_set_path_effect(skity_paint paint, skity_path_effect effect) {
  auto* p = paint_of(paint);
  if (p == nullptr) return;
  if (effect != nullptr) {
    p->SetPathEffect(
        skity::capi::get_impl<skity_path_effect_s, skity::PathEffect>(
            effect, SKITY_OBJECT_TYPE_PATH_EFFECT));
  } else {
    p->SetPathEffect(nullptr);
  }
}

void skity_paint_set_typeface(skity_paint paint, skity_typeface typeface) {
  auto* p = paint_of(paint);
  if (p == nullptr) return;
  if (typeface != nullptr) {
    p->SetTypeface(skity::capi::get_impl<skity_typeface_s, skity::Typeface>(
        typeface, SKITY_OBJECT_TYPE_TYPEFACE));
  } else {
    p->SetTypeface(nullptr);
  }
}

skity_color skity_paint_get_color(skity_paint paint) {
  auto* p = paint_of(paint);
  return p ? static_cast<skity_color>(p->GetColor()) : 0;
}

void skity_paint_get_stroke_color(skity_paint paint, skity_vec4* out) {
  auto* p = paint_of(paint);
  if (p == nullptr || out == nullptr) return;
  skity::Vector c = p->GetStrokeColor();
  out->e[0] = c.e[0];
  out->e[1] = c.e[1];
  out->e[2] = c.e[2];
  out->e[3] = c.e[3];
}

void skity_paint_get_fill_color(skity_paint paint, skity_vec4* out) {
  auto* p = paint_of(paint);
  if (p == nullptr || out == nullptr) return;
  skity::Vector c = p->GetFillColor();
  out->e[0] = c.e[0];
  out->e[1] = c.e[1];
  out->e[2] = c.e[2];
  out->e[3] = c.e[3];
}

skity_paint_style skity_paint_get_style(skity_paint paint) {
  auto* p = paint_of(paint);
  return p ? static_cast<skity_paint_style>(p->GetStyle())
           : SKITY_PAINT_STYLE_FILL;
}

float skity_paint_get_stroke_width(skity_paint paint) {
  auto* p = paint_of(paint);
  return p ? p->GetStrokeWidth() : 0.f;
}

float skity_paint_get_stroke_miter(skity_paint paint) {
  auto* p = paint_of(paint);
  return p ? p->GetStrokeMiter() : 0.f;
}

skity_paint_cap skity_paint_get_stroke_cap(skity_paint paint) {
  auto* p = paint_of(paint);
  return p ? static_cast<skity_paint_cap>(p->GetStrokeCap())
           : SKITY_PAINT_CAP_BUTT;
}

skity_paint_join skity_paint_get_stroke_join(skity_paint paint) {
  auto* p = paint_of(paint);
  return p ? static_cast<skity_paint_join>(p->GetStrokeJoin())
           : SKITY_PAINT_JOIN_MITER;
}

float skity_paint_get_text_size(skity_paint paint) {
  auto* p = paint_of(paint);
  return p ? p->GetTextSize() : 0.f;
}

skity_blend_mode skity_paint_get_blend_mode(skity_paint paint) {
  auto* p = paint_of(paint);
  return p ? static_cast<skity_blend_mode>(p->GetBlendMode())
           : SKITY_BLEND_MODE_SRC_OVER;
}

uint8_t skity_paint_get_alpha(skity_paint paint) {
  auto* p = paint_of(paint);
  return p ? p->GetAlpha() : 0;
}

uint32_t skity_paint_is_anti_alias(skity_paint paint) {
  auto* p = paint_of(paint);
  return (p != nullptr && p->IsAntiAlias()) ? 1u : 0u;
}

skity_shader skity_paint_get_shader(skity_paint paint) {
  auto* p = paint_of(paint);
  if (p == nullptr) return nullptr;
  auto sp = p->GetShader();
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_shader_s>(
      SKITY_OBJECT_TYPE_SHADER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_color_filter skity_paint_get_color_filter(skity_paint paint) {
  auto* p = paint_of(paint);
  if (p == nullptr) return nullptr;
  auto sp = p->GetColorFilter();
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_color_filter_s>(
      SKITY_OBJECT_TYPE_COLOR_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_image_filter skity_paint_get_image_filter(skity_paint paint) {
  auto* p = paint_of(paint);
  if (p == nullptr) return nullptr;
  auto sp = p->GetImageFilter();
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_image_filter_s>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_mask_filter skity_paint_get_mask_filter(skity_paint paint) {
  auto* p = paint_of(paint);
  if (p == nullptr) return nullptr;
  auto sp = p->GetMaskFilter();
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_mask_filter_s>(
      SKITY_OBJECT_TYPE_MASK_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_path_effect skity_paint_get_path_effect(skity_paint paint) {
  auto* p = paint_of(paint);
  if (p == nullptr) return nullptr;
  auto sp = p->GetPathEffect();
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_path_effect_s>(
      SKITY_OBJECT_TYPE_PATH_EFFECT, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_typeface skity_paint_get_typeface(skity_paint paint) {
  auto* p = paint_of(paint);
  if (p == nullptr) return nullptr;
  auto sp = p->GetTypeface();
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_typeface_s>(
      SKITY_OBJECT_TYPE_TYPEFACE, SKITY_HANDLE_OWNING, std::move(sp));
}

}  // extern "C"
