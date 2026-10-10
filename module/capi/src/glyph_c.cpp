// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_glyph.h>

#include <skity/graphic/path.hpp>
#include <skity/text/glyph.hpp>

#include "handle.hpp"

namespace {

skity::GlyphData* glyph_data_of(skity_glyph_data handle) {
  auto* w = skity::capi::resolve<skity_glyph_data_s>(
      handle, SKITY_OBJECT_TYPE_GLYPH_DATA);
  return w ? static_cast<skity::GlyphData*>(w->impl.get()) : nullptr;
}

skity_bitmap_format to_bitmap_format(skity::BitmapFormat format) {
  switch (format) {
    case skity::BitmapFormat::kGray8:
      return SKITY_BITMAP_FORMAT_GRAY8;
    case skity::BitmapFormat::kBGRA8:
      return SKITY_BITMAP_FORMAT_BGRA8;
    case skity::BitmapFormat::kRGBA8:
      return SKITY_BITMAP_FORMAT_RGBA8;
    case skity::BitmapFormat::kUnknown:
      break;
  }
  return SKITY_BITMAP_FORMAT_UNKNOWN;
}

}  // namespace

extern "C" {

void skity_glyph_data_destroy(skity_glyph_data glyph) {
  skity::capi::destroy_handle<skity_glyph_data_s>(glyph,
                                                  SKITY_OBJECT_TYPE_GLYPH_DATA);
}

uint16_t skity_glyph_data_get_id(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->Id() : 0;
}

float skity_glyph_data_get_advance_x(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->AdvanceX() : 0.f;
}

float skity_glyph_data_get_advance_y(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->AdvanceY() : 0.f;
}

float skity_glyph_data_get_width(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->GetWidth() : 0.f;
}

float skity_glyph_data_get_height(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->GetHeight() : 0.f;
}

float skity_glyph_data_get_left(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->GetLeft() : 0.f;
}

float skity_glyph_data_get_top(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->GetTop() : 0.f;
}

float skity_glyph_data_get_hori_bearing_x(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->GetHoriBearingX() : 0.f;
}

float skity_glyph_data_get_hori_bearing_y(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->GetHoriBearingY() : 0.f;
}

float skity_glyph_data_get_y_min(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->GetYMin() : 0.f;
}

float skity_glyph_data_get_y_max(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->GetYMax() : 0.f;
}

float skity_glyph_data_get_font_size(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->FontSize() : 0.f;
}

float skity_glyph_data_get_fixed_size(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  return g ? g->FixedSize() : 0.f;
}

uint32_t skity_glyph_data_get_format(skity_glyph_data glyph,
                                     skity_glyph_format* out) {
  auto* g = glyph_data_of(glyph);
  if (g == nullptr || out == nullptr) {
    return 0u;
  }
  auto format = g->GetFormat();
  if (!format.has_value()) {
    return 0u;
  }
  *out = static_cast<skity_glyph_format>(*format);
  return 1u;
}

skity_path skity_glyph_data_get_path(skity_glyph_data glyph) {
  auto* g = glyph_data_of(glyph);
  if (g == nullptr) {
    return nullptr;
  }
  // Non-owning: the path is a member of the cached GlyphData and shares its
  // eviction-based lifetime; skity_path_destroy only reclaims the wrapper.
  std::shared_ptr<skity::Path> impl(const_cast<skity::Path*>(&g->GetPath()),
                                    [](skity::Path*) {});
  return skity::capi::alloc_handle<skity_path_s>(SKITY_OBJECT_TYPE_PATH, 0u,
                                                 std::move(impl));
}

uint32_t skity_glyph_data_get_bitmap(skity_glyph_data glyph,
                                     skity_glyph_bitmap* out) {
  auto* g = glyph_data_of(glyph);
  if (g == nullptr || out == nullptr) {
    return 0u;
  }
  const skity::GlyphBitmapData& image = g->Image();
  if (image.buffer == nullptr) {
    return 0u;
  }
  out->origin_x = image.origin_x;
  out->origin_y = image.origin_y;
  out->origin_x_for_raster = image.origin_x_for_raster;
  out->origin_y_for_raster = image.origin_y_for_raster;
  out->width = image.width;
  out->height = image.height;
  out->buffer = image.buffer;
  out->row_bytes = image.RowBytes();
  out->format = to_bitmap_format(image.format);
  return 1u;
}

}  // extern "C"
