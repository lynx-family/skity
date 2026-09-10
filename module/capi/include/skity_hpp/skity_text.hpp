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
#include <skity_hpp/skity_paint.hpp>
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

 private:
  explicit Typeface(skity_typeface h) : OwnHandle(h) {}
  friend class FontManager;
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

 private:
  explicit TypefaceDelegate(skity_typeface_delegate h) : OwnHandle(h) {}
};

/** Font manager wrapper: family enumeration and typeface lookup. */
class FontManager
    : public detail::OwnHandle<skity_font_manager, skity_font_manager_destroy> {
 public:
  /** Reference to the platform default manager (legacy naming). */
  static FontManager RefDefault() {
    return FontManager(skity_font_manager_ref_default());
  }

  /** Find a typeface in @p name covering @p character; empty if none. */
  Typeface MatchFamilyStyleCharacter(const char* name, FontStyle style,
                                     const char** bcp47, int32_t bcp47_count,
                                     uint32_t character) {
    return Typeface(skity_font_manager_match_family_style_character(
        get(), name, style, bcp47, bcp47_count, character));
  }

 private:
  explicit FontManager(skity_font_manager h) : OwnHandle(h) {}
};

/** Font wrapper: typeface + size + rasterization hints, used for
 *  DrawGlyphs and glyph-run text blobs. */
class Font : public detail::OwnHandle<skity_font, skity_font_destroy> {
 public:
  Font() : OwnHandle(skity_font_create()) {}

  /** @p typeface NULL selects the default; sizes <= 0 are ignored. */
  explicit Font(skity_typeface typeface, float size)
      : OwnHandle(skity_font_create_with_typeface(typeface, size)) {}

  void SetTypeface(skity_typeface typeface) {
    skity_font_set_typeface(get(), typeface);
  }

  void SetSize(float size) { skity_font_set_size(get(), size); }
  float GetSize() const { return skity_font_get_size(get()); }

  void GetMetrics(skity_font_metrics* out) const {
    skity_font_get_metrics(get(), out);
  }
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
