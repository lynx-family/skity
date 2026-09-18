// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXT_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXT_HPP

// Text wrappers of the header-only RAII layer: Typeface (abstract in C++,
// resolved through a concrete port by the C layer), Font (typeface + size +
// rasterization hints), and immutable TextBlob. See skity.hpp for the
// layer's design.

#include <skity_c/skity_font.h>
#include <skity_c/skity_text.h>

#include <cstddef>
#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_data.hpp>
#include <skity_hpp/skity_paint.hpp>
#include <string>
#include <vector>

namespace skity {
namespace raii {

/** Font style POD passthrough (weight / width / slant), mirroring the
 *  legacy skity::FontStyle default-constructible shape. */
using FontStyle = skity_font_style;

/** Typeface wrapper. Typeface is abstract in C++; the C layer resolves it
 *  through a concrete port, so this stays a plain handle owner. */
class Typeface
    : public detail::OwnHandle<skity_typeface, skity_typeface_destroy> {
 public:
  /** Load a typeface from a font file path; empty on failure. */
  static Typeface MakeFromFile(const char* path) {
    return Typeface(skity_typeface_make_from_file(path));
  }

  /** Load a typeface from font data; @p data must outlive the typeface. */
  static Typeface MakeFromData(skity_data data) {
    return Typeface(skity_typeface_make_from_data(data));
  }

  /** The platform default typeface (legacy naming). */
  static Typeface GetDefaultTypeface() {
    return Typeface(skity_typeface_get_default());
  }

  /**
   * Adopt an owning C handle (e.g. from skity_paint_get_typeface or
   * Font::GetTypeface); destroying the wrapper releases the reference.
   */
  static Typeface Adopt(skity_typeface h) { return Typeface(h); }

  /** Map code points to glyph ids (unmapped ones become 0). */
  void UnicharsToGlyphs(const uint32_t uni[], int32_t count,
                        uint16_t glyphs[]) const {
    skity_typeface_unichars_to_glyphs(get(), uni, count, glyphs);
  }
  /** Single-code-point convenience; 0 when @p unichar has no glyph. */
  uint16_t UnicharToGlyph(uint32_t unichar) const {
    return skity_typeface_unichar_to_glyph(get(), unichar);
  }

 private:
  explicit Typeface(skity_typeface h) : OwnHandle(h) {}
  friend class FontManager;
  friend class FontStyleSet;
};

/**
 * Fallback delegate wrapper: decides which typeface renders a code point the
 * paint's typeface does not cover (CJK / emoji mixing). Pass to
 * TextBlob::MakeWithDelegate.
 */
class TypefaceDelegate
    : public detail::OwnHandle<skity_typeface_delegate,
                               skity_typeface_delegate_destroy> {
 public:
  /**
   * Ordered fallback list: for an uncovered code point, the first typeface
   * containing a glyph for it is used. The delegate shares the typefaces, so
   * the passed wrappers may be released immediately afterwards.
   */
  static TypefaceDelegate CreateSimpleFallbackDelegate(
      const Typeface* typefaces, uint32_t count) {
    std::vector<skity_typeface> handles(count);
    for (uint32_t i = 0; i < count; i++) {
      handles[i] = typefaces[i].get();
    }
    return TypefaceDelegate(
        skity_typeface_delegate_create_simple(handles.data(), count));
  }

  /**
   * Custom fallback policy: @p fallback returns the typeface to render
   * @p code_point (an empty handle keeps the paint's typeface). @p release
   * (optional) is invoked when the delegate is destroyed.
   */
  static TypefaceDelegate CreateFallbackDelegate(
      skity_typeface_fallback_fn fallback, void* userdata,
      void (*release)(void* userdata) = nullptr) {
    return TypefaceDelegate(
        skity_typeface_delegate_create_fallback(fallback, userdata, release));
  }

 private:
  explicit TypefaceDelegate(skity_typeface_delegate h) : OwnHandle(h) {}
};

/** One family of a FontManager: variants enumerated by style. */
class FontStyleSet : public detail::OwnHandle<skity_font_style_set,
                                              skity_font_style_set_destroy> {
 public:
  /** Number of variants in the set. */
  int32_t GetCount() const { return skity_font_style_set_count(get()); }

  /** Style (and optional display name) of the variant at @p index. */
  FontStyle GetStyle(int32_t index, std::string* name = nullptr) const {
    FontStyle style{};
    char buffer[256];
    skity_font_style_set_get_style(get(), index, &style, buffer,
                                   sizeof(buffer));
    if (name != nullptr) {
      *name = buffer;
    }
    return style;
  }

  /** Create the typeface of the variant at @p index; empty on failure. */
  Typeface CreateTypeface(int32_t index) const {
    return Typeface(skity_font_style_set_create_typeface(get(), index));
  }

  /** Variant closest to @p style; empty if the family has none. */
  Typeface MatchStyle(FontStyle style) const {
    return Typeface(skity_font_style_set_match_style(get(), style));
  }

 private:
  explicit FontStyleSet(skity_font_style_set h) : OwnHandle(h) {}
  friend class FontManager;
};

/** Font manager wrapper: family enumeration and typeface lookup. */
class FontManager
    : public detail::OwnHandle<skity_font_manager, skity_font_manager_destroy> {
 public:
  /** Reference to the platform default manager (legacy naming). */
  static FontManager RefDefault() {
    return FontManager(skity_font_manager_ref_default());
  }

  /** Number of known font families. */
  int32_t GetFamilyCount() const {
    return skity_font_manager_count_families(get());
  }

  /** Family name at @p index; empty string when out of range. */
  std::string GetFamilyName(int32_t index) const {
    int32_t length =
        skity_font_manager_get_family_name(get(), index, nullptr, 0);
    if (length <= 0) {
      return {};
    }
    // The length includes the NUL terminator the C layer writes.
    std::string name(static_cast<size_t>(length), '\0');
    skity_font_manager_get_family_name(get(), index, &name[0], length);
    name.resize(std::string(name.c_str()).size());
    return name;
  }

  /** Style set of the family at @p index; empty wrapper when out of range. */
  FontStyleSet CreateStyleSet(int32_t index) const {
    return FontStyleSet(skity_font_manager_create_style_set(get(), index));
  }

  /** Style set of the named family; empty wrapper when not found. */
  FontStyleSet MatchFamily(const char* name) const {
    return FontStyleSet(skity_font_manager_match_family(get(), name));
  }

  /** Typeface closest to @p style within @p name; empty when not found. */
  Typeface MatchFamilyStyle(const char* name, FontStyle style) const {
    return Typeface(skity_font_manager_match_family_style(get(), name, style));
  }

  /** Find a typeface in @p name covering @p character; empty if none. */
  Typeface MatchFamilyStyleCharacter(const char* name, FontStyle style,
                                     const char** bcp47, int32_t bcp47_count,
                                     uint32_t character) const {
    return Typeface(skity_font_manager_match_family_style_character(
        get(), name, style, bcp47, bcp47_count, character));
  }

  /**
   * Load a typeface through this manager from a font collection file
   * (@p ttc_index selects the face inside a .ttc).
   */
  Typeface MakeFromFile(const char* path, int32_t ttc_index = 0) const {
    return Typeface(skity_font_manager_make_from_file(get(), path, ttc_index));
  }

  /** Load a typeface through this manager from in-memory font data. */
  Typeface MakeFromData(const Data& data, int32_t ttc_index = 0) const {
    return Typeface(
        skity_font_manager_make_from_data(get(), data.get(), ttc_index));
  }

  /** The default typeface for @p style; empty when none is available. */
  Typeface GetDefaultTypeface(FontStyle style) const {
    return Typeface(skity_font_manager_get_default_typeface(get(), style));
  }

 private:
  explicit FontManager(skity_font_manager h) : OwnHandle(h) {}
};

/** Font wrapper: typeface + size + rasterization hints, used for
 *  DrawGlyphs and glyph-run text blobs. */
class Font : public detail::OwnHandle<skity_font, skity_font_destroy> {
 public:
  /** Hinting level, mirroring the legacy skity::Font::FontHinting. */
  enum class FontHinting : uint32_t {
    kNone = SKITY_FONT_HINTING_NONE,
    kSlight = SKITY_FONT_HINTING_SLIGHT,
    kNormal = SKITY_FONT_HINTING_NORMAL,
    kFull = SKITY_FONT_HINTING_FULL,
  };

  /** Glyph edge rendering, mirroring the legacy skity::Font::Edging. */
  enum class Edging : uint32_t {
    kAlias = SKITY_FONT_EDGING_ALIAS,
    kAntiAlias = SKITY_FONT_EDGING_ANTI_ALIAS,
    kSubpixelAntiAlias = SKITY_FONT_EDGING_SUBPIXEL_ANTI_ALIAS,
  };

  Font() : OwnHandle(skity_font_create()) {}

  /** @p typeface NULL selects the default; sizes <= 0 are ignored. */
  explicit Font(skity_typeface typeface, float size)
      : OwnHandle(skity_font_create_with_typeface(typeface, size)) {}

  /** Typeface + size + horizontal axis transforms (scale 1 / skew 0 = off). */
  Font(skity_typeface typeface, float size, float scale_x, float skew_x)
      : OwnHandle(skity_font_create_with_typeface_scale(typeface, size, scale_x,
                                                        skew_x)) {}

  void SetTypeface(skity_typeface typeface) {
    skity_font_set_typeface(get(), typeface);
  }
  void SetTypeface(const Typeface& typeface) { SetTypeface(typeface.get()); }

  /** Owning reference to the bound typeface; empty when none is set. */
  Typeface GetTypeface() const {
    return Typeface::Adopt(skity_font_get_typeface(get()));
  }

  void SetSize(float size) { skity_font_set_size(get(), size); }
  float GetSize() const { return skity_font_get_size(get()); }

  void SetScaleX(float scale_x) { skity_font_set_scale_x(get(), scale_x); }
  float GetScaleX() const { return skity_font_get_scale_x(get()); }

  void SetSkewX(float skew_x) { skity_font_set_skew_x(get(), skew_x); }
  float GetSkewX() const { return skity_font_get_skew_x(get()); }

  /** Copy of this font configured with @p size (legacy make_with_size). */
  Font MakeWithSize(float size) const {
    return Font(skity_font_make_with_size(get(), size));
  }

  void SetHinting(FontHinting hinting) {
    skity_font_set_hinting(get(), static_cast<skity_font_hinting>(hinting));
  }
  FontHinting GetHinting() const {
    return static_cast<FontHinting>(skity_font_get_hinting(get()));
  }

  void SetEdging(Edging edging) {
    skity_font_set_edging(get(), static_cast<skity_font_edging>(edging));
  }
  Edging GetEdging() const {
    return static_cast<Edging>(skity_font_get_edging(get()));
  }

  void SetSubpixel(bool enable) {
    skity_font_set_subpixel(get(), enable ? 1u : 0u);
  }
  bool IsSubpixel() const { return skity_font_is_subpixel(get()) != 0; }

  void SetForceAutoHinting(bool enable) {
    skity_font_set_force_auto_hinting(get(), enable ? 1u : 0u);
  }
  bool IsForceAutoHinting() const {
    return skity_font_is_force_auto_hinting(get()) != 0;
  }

  void SetEmbeddedBitmaps(bool enable) {
    skity_font_set_embedded_bitmaps(get(), enable ? 1u : 0u);
  }
  bool IsEmbeddedBitmaps() const {
    return skity_font_is_embedded_bitmaps(get()) != 0;
  }

  void SetLinearMetrics(bool enable) {
    skity_font_set_linear_metrics(get(), enable ? 1u : 0u);
  }
  bool IsLinearMetrics() const {
    return skity_font_is_linear_metrics(get()) != 0;
  }

  void SetEmbolden(bool enable) {
    skity_font_set_embolden(get(), enable ? 1u : 0u);
  }
  bool IsEmbolden() const { return skity_font_is_embolden(get()) != 0; }

  void SetBaselineSnap(bool enable) {
    skity_font_set_baseline_snap(get(), enable ? 1u : 0u);
  }
  bool IsBaselineSnap() const {
    return skity_font_is_baseline_snap(get()) != 0;
  }

  void GetMetrics(skity_font_metrics* out) const {
    skity_font_get_metrics(get(), out);
  }

  /** Advance widths of @p count glyphs; @p widths holds @p count entries. */
  void GetWidths(const uint16_t glyphs[], int32_t count, float widths[]) const {
    skity_font_get_widths(get(), glyphs, count, widths);
  }

 private:
  explicit Font(skity_font h) : OwnHandle(h) {}
};

/** Immutable laid-out text. Build from a UTF-8 string + paint (size and
 *  typeface taken from the paint) or from pre-shaped glyph runs. */
class TextBlob
    : public detail::OwnHandle<skity_text_blob, skity_text_blob_destroy> {
 public:
  /** Layout @p text with the paint's text size and typeface. */
  static TextBlob Make(const char* text, const Paint& paint) {
    return TextBlob(skity_text_blob_create(text, paint.get()));
  }

  /**
   * Layout @p text routing uncovered code points through @p delegate for
   * multi-font fallback (legacy TextBlobBuilder shape). A NULL delegate is
   * equivalent to Make.
   */
  static TextBlob MakeWithDelegate(const char* text, const Paint& paint,
                                   skity_typeface_delegate delegate) {
    return TextBlob(
        skity_text_blob_create_with_delegate(text, paint.get(), delegate));
  }

  /**
   * Build from pre-shaped glyphs (the proven caller path: consumers run
   * their own shaping and inject glyph ids + positions). Arrays hold
   * @p count elements; pos_x/pos_y may be NULL for natural advances.
   */
  static TextBlob MakeFromGlyphs(const Font& font, const uint16_t* glyph_ids,
                                 const float* pos_x, const float* pos_y,
                                 size_t count) {
    return TextBlob(skity_text_blob_create_from_glyphs(font.get(), glyph_ids,
                                                       pos_x, pos_y, count));
  }

  /** Bounding box of a glyph run without building a blob. */
  static Rect ComputeRunBounds(int32_t count, const uint16_t* glyphs,
                               const float* pos_x, const float* pos_y,
                               const Font& font, const Paint& paint) {
    Rect out;
    skity_text_blob_compute_bounds(count, glyphs, pos_x, pos_y, font.get(),
                                   paint.get(), &out);
    return out;
  }

  Rect GetBounds() const {
    Rect out;
    skity_text_blob_get_bounds(get(), &out);
    return out;
  }

 private:
  explicit TextBlob(skity_text_blob h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXT_HPP
