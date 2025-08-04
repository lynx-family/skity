/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_TYPEFACE_HPP
#define INCLUDE_SKITY_TEXT_TYPEFACE_HPP

#include <atomic>
#include <functional>
#include <memory>
#include <skity/io/data.hpp>
#include <skity/macros.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_metrics.hpp>
#include <skity/text/font_style.hpp>
#include <skity/text/glyph.hpp>
#include <unordered_map>

namespace skity {

/** Machine endian. */
typedef uint32_t FontTableTag;

using TypefaceID = uint32_t;

class ScalerContext;
class ScalerContextDesc;
class Descriptor;
class SKITY_API Typeface : public std::enable_shared_from_this<Typeface> {
 public:
  virtual ~Typeface() = default;

  /** Returns the typeface's intrinsic style attributes. */
  FontStyle GetFontStyle() const { return font_style_; }

  /** Returns true if style() has the kBold bit set. */
  bool IsBold() const {
    return font_style_.weight() >= FontStyle::kSemiBold_Weight;
  }

  /** Returns true if style() has the kItalic bit set. */
  bool IsItalic() const {
    return font_style_.slant() != FontStyle::kUpright_Slant;
  }

  TypefaceID TypefaceId() { return typeface_id_; }

  void UnicharsToGlyphs(const uint32_t uni[], int count,
                        GlyphID glyphs[]) const;
  GlyphID UnicharToGlyph(uint32_t unichar) const;

  /** Return the number of tables in the font. */
  int CountTables() const;

  /** Copy into tags[] (allocated by the caller) the list of table tags in
   *  the font, and return the number. This will be the same as CountTables()
   *  or 0 if an error occured. If tags == NULL, this only returns the count
   *  (the same as calling CountTables()).
   */
  int GetTableTags(FontTableTag tags[]) const;

  /** Given a table tag, return the size of its contents, or 0 if not present
   */
  size_t GetTableSize(FontTableTag) const;

  /** Copy the contents of a table into data (allocated by the caller). Note
   *  that the contents of the table will be in their native endian order
   *  (which for most truetype tables is big endian). If the table tag is
   *  not found, or there is an error copying the data, then 0 is returned.
   *  If this happens, it is possible that some or all of the memory pointed
   *  to by data may have been written to, even though an error has occured.
   *
   *  @param tag  The table tag whose contents are to be copied
   *  @param offset The offset in bytes into the table's contents where the
   *  copy should start from.
   *  @param length The number of bytes, starting at offset, of table data
   *  to copy.
   *  @param data storage address where the table contents are copied to
   *  @return the number of bytes actually copied into data. If offset+length
   *  exceeds the table's size, then only the bytes up to the table's
   *  size are actually copied, and this is the value returned. If
   *  offset > the table's size, or tag is not a valid table,
   *  then 0 is returned.
   */
  size_t GetTableData(FontTableTag tag, size_t offset, size_t length,
                      void* data) const;

  Data* GetData() { return this->OnGetData(); }

  static std::shared_ptr<Typeface> MakeFromData(
      std::shared_ptr<Data> const& data);
  static std::shared_ptr<Typeface> MakeFromFile(const char* path);

  /** Returns the default normal typeface, which is never nullptr. */
  static std::shared_ptr<Typeface> GetDefaultTypeface(
      class FontStyle font_style = skity::FontStyle());

  bool ContainGlyph(Unichar code_point) const;

  void GetFontMetrics(FontMetrics& metrics, float font_size);

  uint32_t GetUnitsPerEm() const;

  /**
   * Whether this font contains color table, usually this means it is a color
   * emoji typeface
   * @return true  If contains color table
   */
  bool ContainsColorTable() const;

  std::unique_ptr<ScalerContext> CreateScalerContext(
      const ScalerContextDesc* desc) const;

  VariationPosition GetVariationDesignPosition() const;
  std::vector<VariationAxis> GetVariationDesignParameters() const;

  std::shared_ptr<Typeface> MakeVariation(const FontArguments& args) const;

 protected:
  explicit Typeface(const FontStyle& style)
      : typeface_id_(NewTypefaceID()), font_style_(style) {}

  virtual int OnGetTableTags(FontTableTag tags[]) const = 0;
  virtual size_t OnGetTableData(FontTableTag, size_t offset, size_t length,
                                void* data) const = 0;

  virtual void OnCharsToGlyphs(const uint32_t* chars, int count,
                               GlyphID glyphs[]) const = 0;

  virtual Data* OnGetData() = 0;
  virtual uint32_t OnGetUPEM() const = 0;

  virtual bool OnContainsColorTable() const = 0;

  virtual std::unique_ptr<ScalerContext> OnCreateScalerContext(
      const ScalerContextDesc* desc) const = 0;

  virtual VariationPosition OnGetVariationDesignPosition() const = 0;
  virtual std::vector<VariationAxis> OnGetVariationDesignParameters() const = 0;

  virtual std::shared_ptr<Typeface> OnMakeVariation(
      const FontArguments& args) const = 0;

  TypefaceID typeface_id_;
  class FontStyle font_style_;
  std::unordered_map<GlyphID, GlyphData> glyph_cache_;

 private:
  class Impl;
  std::unique_ptr<Impl, std::function<void(Impl*)>> impl_;

  static TypefaceID NewTypefaceID() {
    static std::atomic<int32_t> nextID{1};
    return nextID.fetch_add(1, std::memory_order_relaxed);
  }
  friend class Font;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_TYPEFACE_HPP
