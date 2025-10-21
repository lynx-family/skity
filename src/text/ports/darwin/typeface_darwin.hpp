// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_DARWIN_TYPEFACE_DARWIN_HPP
#define SRC_TEXT_PORTS_DARWIN_TYPEFACE_DARWIN_HPP
#include <skity/macros.hpp>

#if !defined(SKITY_MACOS) && !defined(SKITY_IOS)
#error "Only IOS or MacOS need this header file"
#endif

#include <CoreText/CoreText.h>

#include <skity/text/typeface.hpp>

#include "src/text/ports/darwin/types_darwin.hpp"

namespace skity {

class TypefaceDarwin : public Typeface {
 public:
  static constexpr FourByteTag kFontFactoryID =
      SetFourByteTag('c', 't', 'x', 't');

  static std::shared_ptr<TypefaceDarwin> Make(const FontStyle &style,
                                              UniqueCTFontRef ct_font);
  static std::shared_ptr<TypefaceDarwin> MakeWithoutCache(
      const FontStyle &style, UniqueCTFontRef ct_font);

  ~TypefaceDarwin() override;

  CTFontRef GetCTFont() const;

 protected:
  TypefaceDarwin(FontStyle const &style, UniqueCTFontRef ct_font);

  int OnGetTableTags(FontTableTag *tags) const override;

  size_t OnGetTableData(FontTableTag tag, size_t offset, size_t length,
                        void *data) const override;

  void OnCharsToGlyphs(const uint32_t *chars, int count,
                       GlyphID *glyphs) const override;

  std::shared_ptr<Data> OnGetData() override;

  uint32_t OnGetUPEM() const override;

  bool OnContainsColorTable() const override;

  std::unique_ptr<ScalerContext> OnCreateScalerContext(
      const ScalerContextDesc *desc) const override;

  VariationPosition OnGetVariationDesignPosition() const override;
  std::vector<VariationAxis> OnGetVariationDesignParameters() const override;

  std::shared_ptr<Typeface> OnMakeVariation(
      const FontArguments &args) const override;

  void OnGetFontDescriptor(FontDescriptor &desc) const override;

 private:
  void SerializeData();

 private:
  UniqueCTFontRef ct_font_;
  bool has_color_glyphs_;
  UniqueCFRef<CFDataRef> ct_data_;
  UniqueCFRef<CFArrayRef> variation_axes_;

  std::shared_ptr<Data> serialized_data_ = {};
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_DARWIN_TYPEFACE_DARWIN_HPP
