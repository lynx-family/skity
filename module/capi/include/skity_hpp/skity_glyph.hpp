// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_GLYPH_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_GLYPH_HPP

// Glyph-data wrapper of the header-only RAII layer: a non-owning view of a
// cached GlyphData produced by Font::LoadGlyphMetrics / LoadGlyphPath /
// LoadGlyphBitmap(Info). The underlying object is owned by skity's global
// scaler-context cache and may be evicted by other glyph loading work — read
// out what you need instead of holding the wrapper long-term. See
// skity_c/skity_glyph.h for the full lifetime caveat.

#include <skity_c/skity_glyph.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_path.hpp>
#include <vector>

namespace skity {
namespace raii {

/** Glyph raster format, mirroring skity::GlyphFormat. */
enum class GlyphFormat : uint32_t {
  kA8 = SKITY_GLYPH_FORMAT_A8,
  kRGBA32 = SKITY_GLYPH_FORMAT_RGBA32,
  kBGRA32 = SKITY_GLYPH_FORMAT_BGRA32,
};

/** Bitmap memory layout, mirroring skity::BitmapFormat. */
enum class BitmapFormat : uint32_t {
  kUnknown = SKITY_BITMAP_FORMAT_UNKNOWN,
  kGray8 = SKITY_BITMAP_FORMAT_GRAY8,
  kBGRA8 = SKITY_BITMAP_FORMAT_BGRA8,
  kRGBA8 = SKITY_BITMAP_FORMAT_RGBA8,
};

/** Bitmap description passthrough; @p buffer is borrowed from the glyph
 *  cache and must not be freed. */
using GlyphBitmap = skity_glyph_bitmap;

/**
 * Non-owning view of a cached glyph: advances, extents, bearings, outline
 * path and rasterized bitmap. Produced by the Font::LoadGlyph* family;
 * destroying the wrapper reclaims the small handle only.
 */
class GlyphData
    : public detail::OwnHandle<skity_glyph_data, skity_glyph_data_destroy> {
 public:
  /** Glyph id this data was loaded for. */
  uint16_t GetId() const { return skity_glyph_data_get_id(get()); }

  /** Advances in font-size units. */
  float GetAdvanceX() const { return skity_glyph_data_get_advance_x(get()); }
  float GetAdvanceY() const { return skity_glyph_data_get_advance_y(get()); }

  /** Extents in font-size units (outline box). */
  float GetWidth() const { return skity_glyph_data_get_width(get()); }
  float GetHeight() const { return skity_glyph_data_get_height(get()); }

  /** Left / top of the outline relative to the origin. */
  float GetLeft() const { return skity_glyph_data_get_left(get()); }
  float GetTop() const { return skity_glyph_data_get_top(get()); }

  /** Horizontal bearings. */
  float GetHoriBearingX() const {
    return skity_glyph_data_get_hori_bearing_x(get());
  }
  float GetHoriBearingY() const {
    return skity_glyph_data_get_hori_bearing_y(get());
  }

  /** Tight vertical extent. */
  float GetYMin() const { return skity_glyph_data_get_y_min(get()); }
  float GetYMax() const { return skity_glyph_data_get_y_max(get()); }

  /** Font size the metrics are expressed in. */
  float GetFontSize() const { return skity_glyph_data_get_font_size(get()); }

  /** Fixed pixel size of a bitmap font glyph, 0 for outline fonts. */
  float GetFixedSize() const { return skity_glyph_data_get_fixed_size(get()); }

  /** Raster format; false when unknown (metrics-only load). */
  bool GetFormat(GlyphFormat* out) const {
    skity_glyph_format raw{};
    if (skity_glyph_data_get_format(get(), &raw) == 0u) return false;
    *out = static_cast<GlyphFormat>(raw);
    return true;
  }

  /**
   * Borrowed outline path (needs LoadGlyphPath). The returned wrapper owns
   * only its small handle — the path itself is a member of the cached glyph
   * and shares its eviction-based lifetime.
   */
  Path GetPath() const { return Path(skity_glyph_data_get_path(get())); }

  /**
   * Bitmap description (needs LoadGlyphBitmap / Info); @p out receives a
   * borrowed buffer pointer. False when no bitmap is available.
   */
  bool GetBitmap(GlyphBitmap* out) const {
    return skity_glyph_data_get_bitmap(get(), out) != 0u;
  }

  /**
   * Adopt a raw handle from the skity_font_load_glyph_* family; destroying
   * the wrapper releases only the small handle, never the cached glyph.
   */
  static GlyphData Adopt(skity_glyph_data h) { return GlyphData(h); }

 private:
  explicit GlyphData(skity_glyph_data h) : OwnHandle(h) {}
};

namespace detail {

/** Adopt an array of raw glyph-data handles filled by the C load family. */
inline std::vector<GlyphData> AdoptGlyphHandles(
    const std::vector<skity_glyph_data>& raw) {
  std::vector<GlyphData> out;
  out.reserve(raw.size());
  for (skity_glyph_data h : raw) {
    out.push_back(GlyphData::Adopt(h));
  }
  return out;
}

}  // namespace detail

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_GLYPH_HPP
