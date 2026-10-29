// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_TEXT_TEXT_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_TEXT_TEXT_HPP

#include <skity/c/skity_font.h>
#include <skity/c/skity_text.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <skity/base.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/io/data.hpp>
#include <skity/text/glyph.hpp>
#include <skity/types.hpp>
#include <string>
#include <vector>

namespace skity {

/** Font style POD passthrough (weight / width / slant), mirroring the
 *  legacy skity::FontStyle default-constructible shape. */
using FontStyle = skity_font_style;

/** Typeface wrapper. Typeface is abstract in C++; the C layer resolves it
 *  through a concrete port, so this stays a plain handle owner. */
class Typeface
    : public detail::OwnHandle<skity_typeface, skity_typeface_destroy> {
 public:
  /** Load a typeface from a font file path; null on failure. */
  static std::shared_ptr<Typeface> MakeFromFile(const char* path) {
    auto handle = skity_typeface_make_from_file(path);
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
  }

  /** Load a typeface from font data; data must outlive the typeface. */
  static std::shared_ptr<Typeface> MakeFromData(
      const std::shared_ptr<Data>& data) {
    auto handle = skity_typeface_make_from_data(data ? data->get() : nullptr);
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
  }

  /** The platform default typeface (legacy naming). */
  static std::shared_ptr<Typeface> GetDefaultTypeface() {
    auto handle = skity_typeface_get_default();
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
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
  /** Single-code-point convenience; 0 when unichar has no glyph. */
  uint16_t UnicharToGlyph(uint32_t unichar) const {
    return skity_typeface_unichar_to_glyph(get(), unichar);
  }

  /** Raw font file bytes; null when none are accessible. */
  std::shared_ptr<Data> GetData() const {
    auto handle = skity_typeface_get_data(get());
    return handle != nullptr ? std::shared_ptr<Data>(new Data(handle))
                             : nullptr;
  }

  /** Font design units per em square (0 on an empty handle). */
  uint32_t GetUnitsPerEm() const {
    return skity_typeface_get_units_per_em(get());
  }

  /** Intrinsic weight / width / slant of the face. */
  FontStyle GetFontStyle() const {
    FontStyle style{};
    skity_typeface_get_font_style(get(), &style);
    return style;
  }

  /** Stable id; equal ids mean the same underlying typeface instance. */
  uint32_t GetUniqueId() const { return skity_typeface_get_unique_id(get()); }

  /** Intrinsic weight >= semibold / non-upright slant. */
  bool IsBold() const { return skity_typeface_is_bold(get()) != 0u; }
  bool IsItalic() const { return skity_typeface_is_italic(get()) != 0u; }

  /** Whether unichar has a glyph in this face. */
  bool ContainGlyph(uint32_t unichar) const {
    return skity_typeface_contain_glyph(get(), unichar) != 0u;
  }

  /** Whether the face carries a color bitmap / COLR table (emoji fonts). */
  bool ContainsColorTable() const {
    return skity_typeface_contains_color_table(get()) != 0u;
  }

  /** Number of font tables. */
  int32_t CountTables() const { return skity_typeface_count_tables(get()); }

  /** FourCC table tags (two-pass: pass NULL to query the count). */
  int32_t GetTableTags(uint32_t* tags, int32_t count) const {
    return skity_typeface_get_table_tags(get(), tags, count);
  }

  /** Size in bytes of table tag, 0 when absent. */
  size_t GetTableSize(uint32_t tag) const {
    return skity_typeface_get_table_size(get(), tag);
  }

  /** Copy a range of table tag into data; bytes actually copied. */
  size_t GetTableData(uint32_t tag, size_t offset, size_t length,
                      void* data) const {
    return skity_typeface_get_table_data(get(), tag, offset, length, data);
  }

  /** Active variation coordinates (empty for static fonts). */
  std::vector<skity_variation_coordinate> GetVariationPosition() const {
    int32_t n = skity_typeface_get_variation_position(get(), nullptr, 0);
    std::vector<skity_variation_coordinate> coords(
        static_cast<size_t>(n > 0 ? n : 0));
    if (n > 0) {
      skity_typeface_get_variation_position(get(), coords.data(), n);
    }
    return coords;
  }

  /** Variation axes the font declares (empty for static fonts). */
  std::vector<skity_variation_axis> GetVariationAxes() const {
    int32_t n = skity_typeface_get_variation_axes(get(), nullptr, 0);
    std::vector<skity_variation_axis> axes(static_cast<size_t>(n > 0 ? n : 0));
    if (n > 0) {
      skity_typeface_get_variation_axes(get(), axes.data(), n);
    }
    return axes;
  }

  /**
   * Instantiate this variable font at coordinates (legacy MakeVariation);
   * empty wrapper for non-variable faces or on failure.
   */
  std::shared_ptr<Typeface> MakeVariation(
      const skity_variation_coordinate* coordinates, uint32_t coordinate_count,
      int32_t collection_index = 0) const {
    skity_font_arguments args{};
    args.collection_index = collection_index;
    args.variation_coordinates = coordinates;
    args.variation_coordinate_count = coordinate_count;
    auto handle = skity_typeface_make_variation(get(), &args);
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
  }

  /**
   * Identity summary (style / collection index / factory id) plus family
   * name; useful as a cache key together with GetUniqueId.
   */
  skity_font_descriptor GetFontDescriptor(std::string* family_name) const {
    skity_font_descriptor desc{};
    if (family_name != nullptr) {
      int32_t length =
          skity_typeface_get_font_descriptor(get(), &desc, nullptr, 0);
      if (length > 0) {
        // The length includes the NUL terminator the C layer writes.
        family_name->assign(static_cast<size_t>(length), '\0');
        skity_typeface_get_font_descriptor(get(), &desc, &(*family_name)[0],
                                           length);
        family_name->resize(std::string(family_name->c_str()).size());
      }
    } else {
      skity_typeface_get_font_descriptor(get(), &desc, nullptr, 0);
    }
    return desc;
  }

 private:
  explicit Typeface(skity_typeface h) : OwnHandle(h) {}
  friend class FontManager;
  friend class FontStyleSet;
  friend class Font;
  friend class Paint;
};

// Out-of-line Paint accessors declared in graphic/paint.hpp (Typeface is
// forward-declared there to keep the include graph one-directional).
inline void Paint::SetTypeface(std::shared_ptr<Typeface> typeface) {
  skity_paint_set_typeface(get(), typeface ? typeface->get() : nullptr);
}
inline std::shared_ptr<Typeface> Paint::GetTypeface() const {
  skity_typeface h = skity_paint_get_typeface(get());
  return h != nullptr ? std::shared_ptr<Typeface>(new Typeface(h)) : nullptr;
}

/**
 * @class TypefaceDelegate
 *
 * Fallback delegate wrapper: decides which typeface renders a code point the
 * paint's typeface does not cover (CJK / emoji mixing). Pass to
 * TextBlob::MakeWithDelegate.
 *
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
  static std::unique_ptr<TypefaceDelegate> CreateSimpleFallbackDelegate(
      const std::vector<std::shared_ptr<Typeface>>& typefaces) {
    std::vector<skity_typeface> handles(typefaces.size());
    for (size_t i = 0; i < typefaces.size(); i++) {
      handles[i] = typefaces[i] ? typefaces[i]->get() : nullptr;
    }
    return std::unique_ptr<TypefaceDelegate>(
        new TypefaceDelegate(skity_typeface_delegate_create_simple(
            handles.data(), static_cast<uint32_t>(handles.size()))));
  }

  /**
   * Custom fallback policy: fallback returns the typeface to render
   * code_point (an empty handle keeps the paint's typeface). release
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

/**
 * @class FontStyleSet
 *
 * One family of a FontManager: variants enumerated by style.
 *
 */
class FontStyleSet : public detail::OwnHandle<skity_font_style_set,
                                              skity_font_style_set_destroy> {
 public:
  /** Number of variants in the set. */
  int32_t GetCount() const { return skity_font_style_set_count(get()); }

  /** Style (and optional display name) of the variant at index. */
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

  /** Create the typeface of the variant at index; null on failure. */
  std::shared_ptr<Typeface> CreateTypeface(int32_t index) const {
    auto handle = skity_font_style_set_create_typeface(get(), index);
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
  }

  /** Variant closest to style; null if the family has none. */
  std::shared_ptr<Typeface> MatchStyle(FontStyle style) const {
    auto handle = skity_font_style_set_match_style(get(), style);
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
  }

 private:
  explicit FontStyleSet(skity_font_style_set h) : OwnHandle(h) {}
  friend class FontManager;
};

/**
 * @class FontManager
 *
 * Font manager wrapper: family enumeration and typeface lookup.
 *
 */
class FontManager
    : public detail::OwnHandle<skity_font_manager, skity_font_manager_destroy> {
 public:
  /** Reference to the platform default manager (legacy naming). */
  static std::shared_ptr<FontManager> RefDefault() {
    auto handle = skity_font_manager_ref_default();
    return handle != nullptr
               ? std::shared_ptr<FontManager>(new FontManager(handle))
               : nullptr;
  }

  /** Number of known font families. */
  int32_t GetFamilyCount() const {
    return skity_font_manager_count_families(get());
  }

  /** Family name at index; empty string when out of range. */
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

  /** Style set of the family at index; null when out of range. */
  std::shared_ptr<FontStyleSet> CreateStyleSet(int32_t index) const {
    auto handle = skity_font_manager_create_style_set(get(), index);
    return handle != nullptr
               ? std::shared_ptr<FontStyleSet>(new FontStyleSet(handle))
               : nullptr;
  }

  /** Style set of the named family; null when not found. */
  std::shared_ptr<FontStyleSet> MatchFamily(const char* name) const {
    auto handle = skity_font_manager_match_family(get(), name);
    return handle != nullptr
               ? std::shared_ptr<FontStyleSet>(new FontStyleSet(handle))
               : nullptr;
  }

  /** Typeface closest to style within name; null when not found. */
  std::shared_ptr<Typeface> MatchFamilyStyle(const char* name,
                                             FontStyle style) const {
    auto handle = skity_font_manager_match_family_style(get(), name, style);
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
  }

  /** Find a typeface in name covering character; null if none. */
  std::shared_ptr<Typeface> MatchFamilyStyleCharacter(
      const char* name, FontStyle style, const char** bcp47,
      int32_t bcp47_count, uint32_t character) const {
    auto handle = skity_font_manager_match_family_style_character(
        get(), name, style, bcp47, bcp47_count, character);
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
  }

  /**
   * Load a typeface through this manager from a font collection file
   * (ttc_index selects the face inside a .ttc).
   */
  std::shared_ptr<Typeface> MakeFromFile(const char* path,
                                         int32_t ttc_index = 0) const {
    auto handle = skity_font_manager_make_from_file(get(), path, ttc_index);
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
  }

  /** Load a typeface through this manager from in-memory font data. */
  std::shared_ptr<Typeface> MakeFromData(const std::shared_ptr<Data>& data,
                                         int32_t ttc_index = 0) const {
    auto handle = skity_font_manager_make_from_data(
        get(), data ? data->get() : nullptr, ttc_index);
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
  }

  /** The default typeface for style; null when none is available. */
  std::shared_ptr<Typeface> GetDefaultTypeface(FontStyle style) const {
    auto handle = skity_font_manager_get_default_typeface(get(), style);
    return handle != nullptr ? std::shared_ptr<Typeface>(new Typeface(handle))
                             : nullptr;
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

  /** shared_ptr constructors, the legacy signatures; a null typeface
   *  selects the default and sizes <= 0 are ignored. */
  explicit Font(std::shared_ptr<Typeface> typeface)
      : OwnHandle(skity_font_create_with_typeface(
            typeface ? typeface->get() : nullptr, 0.f)) {}

  explicit Font(std::shared_ptr<Typeface> typeface, float size)
      : OwnHandle(skity_font_create_with_typeface(
            typeface ? typeface->get() : nullptr, size)) {}

  Font(std::shared_ptr<Typeface> typeface, float size, float scale_x,
       float skew_x)
      : OwnHandle(skity_font_create_with_typeface_scale(
            typeface ? typeface->get() : nullptr, size, scale_x, skew_x)) {}

  /**
   * Copy semantics of the value-type skity::Font: parameters are
   * duplicated while the typeface is shared — identical to copying a
   * skity::Font. Move stays cheap.
   */
  Font(const Font& other) : OwnHandle(skity_font_clone(other.get())) {}

  Font& operator=(const Font& other) {
    if (this != &other) reset(skity_font_clone(other.get()));
    return *this;
  }

  Font(Font&&) = default;
  Font& operator=(Font&&) = default;

  void SetTypeface(std::shared_ptr<Typeface> typeface) {
    skity_font_set_typeface(get(), typeface ? typeface->get() : nullptr);
  }

  /** Owning shared reference to the bound typeface; null when none is set. */
  std::shared_ptr<Typeface> GetTypeface() const {
    skity_typeface h = skity_font_get_typeface(get());
    return h != nullptr ? std::shared_ptr<Typeface>(new Typeface(h)) : nullptr;
  }

  void SetSize(float size) { skity_font_set_size(get(), size); }
  float GetSize() const { return skity_font_get_size(get()); }

  void SetScaleX(float scale_x) { skity_font_set_scale_x(get(), scale_x); }
  float GetScaleX() const { return skity_font_get_scale_x(get()); }

  void SetSkewX(float skew_x) { skity_font_set_skew_x(get(), skew_x); }
  float GetSkewX() const { return skity_font_get_skew_x(get()); }

  /** Copy of this font configured with size (legacy make_with_size). */
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

  /** Advance widths of count glyphs; widths holds count entries. */
  void GetWidths(const uint16_t glyphs[], int32_t count, float widths[]) const {
    skity_font_get_widths(get(), glyphs, count, widths);
  }

  /**
   * Axis-aligned bounds of count glyphs at the font's current size
   * (baseline-relative, y growing downwards); bounds holds count
   * entries. Companion of GetWidths, mirroring the legacy
   * Font::GetWidthsBounds bounds-only overload.
   */
  void GetBounds(const uint16_t glyphs[], int32_t count, Rect bounds[]) const {
    skity_font_get_bounds(get(), glyphs, count, bounds);
  }

  /**
   * Glyph-data loading family (custom shaping / atlas pipelines). Each
   * returns non-owning GlyphData views into skity's global glyph cache —
   * read what you need promptly; see skity_glyph.hpp for the eviction
   * caveat.
   */
  ///@{
  std::vector<GlyphData> LoadGlyphMetrics(const uint16_t glyphs[],
                                          uint32_t count) const {
    std::vector<skity_glyph_data> raw(count, nullptr);
    skity_font_load_glyph_metrics(get(), glyphs, count, raw.data());
    return detail::AdoptGlyphHandles(raw);
  }

  std::vector<GlyphData> LoadGlyphPath(const uint16_t glyphs[],
                                       uint32_t count) const {
    std::vector<skity_glyph_data> raw(count, nullptr);
    skity_font_load_glyph_path(get(), glyphs, count, raw.data());
    return detail::AdoptGlyphHandles(raw);
  }

  std::vector<GlyphData> LoadGlyphBitmap(
      const uint16_t glyphs[], uint32_t count, const Paint& paint,
      float context_scale, const Matrix* transform = nullptr) const {
    std::vector<skity_glyph_data> raw(count, nullptr);
    skity_font_load_glyph_bitmap(get(), glyphs, count, raw.data(), paint.get(),
                                 context_scale, transform);
    return detail::AdoptGlyphHandles(raw);
  }

  std::vector<GlyphData> LoadGlyphBitmapInfo(
      const uint16_t glyphs[], uint32_t count, const Paint& paint,
      float context_scale, const Matrix* transform = nullptr) const {
    std::vector<skity_glyph_data> raw(count, nullptr);
    skity_font_load_glyph_bitmap_info(get(), glyphs, count, raw.data(),
                                      paint.get(), context_scale, transform);
    return detail::AdoptGlyphHandles(raw);
  }
  ///@}

 private:
  explicit Font(skity_font h) : OwnHandle(h) {}
};

/** Immutable laid-out text. Build from a UTF-8 string + paint (size and
 *  typeface taken from the paint) or from pre-shaped glyph runs. */
class TextBlob
    : public detail::OwnHandle<skity_text_blob, skity_text_blob_destroy> {
 public:
  /** Layout text with the paint's text size and typeface. */
  static std::shared_ptr<TextBlob> Make(const char* text, const Paint& paint) {
    auto handle = skity_text_blob_create(text, paint.get());
    return handle != nullptr ? std::shared_ptr<TextBlob>(new TextBlob(handle))
                             : nullptr;
  }

  /**
   * Layout text routing uncovered code points through delegate for
   * multi-font fallback (legacy TextBlobBuilder shape). A NULL delegate is
   * equivalent to Make.
   */
  static std::shared_ptr<TextBlob> MakeWithDelegate(
      const char* text, const Paint& paint, const TypefaceDelegate* delegate) {
    auto handle = skity_text_blob_create_with_delegate(
        text, paint.get(), delegate ? delegate->get() : nullptr);
    return handle != nullptr ? std::shared_ptr<TextBlob>(new TextBlob(handle))
                             : nullptr;
  }

  /**
   * Build from pre-shaped glyphs (the proven caller path: consumers run
   * their own shaping and inject glyph ids + positions). Arrays hold
   * count elements; pos_x/pos_y may be NULL for natural advances.
   */
  static std::shared_ptr<TextBlob> MakeFromGlyphs(const Font& font,
                                                  const uint16_t* glyph_ids,
                                                  const float* pos_x,
                                                  const float* pos_y,
                                                  size_t count) {
    auto handle = skity_text_blob_create_from_glyphs(font.get(), glyph_ids,
                                                     pos_x, pos_y, count);
    return handle != nullptr ? std::shared_ptr<TextBlob>(new TextBlob(handle))
                             : nullptr;
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

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_TEXT_TEXT_HPP
