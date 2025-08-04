// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_TYPEFACE_EMPTY_HPP
#define SRC_TEXT_PORTS_TYPEFACE_EMPTY_HPP

#include <skity/text/font_style.hpp>
#include <skity/text/typeface.hpp>

namespace skity {

class TypefaceEmpty : public Typeface {
 public:
  TypefaceEmpty() : Typeface(FontStyle()) {}
  ~TypefaceEmpty() override = default;

 public:
 protected:
  int OnGetTableTags(FontTableTag *tags) const override;
  size_t OnGetTableData(FontTableTag tag, size_t offset, size_t length,
                        void *data) const override;
  void OnCharsToGlyphs(const uint32_t *chars, int count,
                       GlyphID *glyphs) const override;
  Data *OnGetData() override;
  uint32_t OnGetUPEM() const override;
  bool OnContainsColorTable() const override;
  std::unique_ptr<ScalerContext> OnCreateScalerContext(
      const ScalerContextDesc *desc) const override;

  VariationPosition OnGetVariationDesignPosition() const override;
  std::vector<VariationAxis> OnGetVariationDesignParameters() const override;

  std::shared_ptr<Typeface> OnMakeVariation(
      const FontArguments &args) const override;
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_TYPEFACE_EMPTY_HPP
