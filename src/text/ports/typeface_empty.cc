// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/typeface_empty.hpp"

#include "src/text/scaler_context.hpp"

namespace skity {
class ScalerContextEmpty : public ScalerContext {
 protected:
  void GenerateMetrics(GlyphData *) override {}
  void GenerateImage(GlyphData *, const StrokeDesc &) override {}
  bool GeneratePath(GlyphData *) override { return true; }
  void GenerateFontMetrics(FontMetrics *) override {}
  uint16_t OnGetFixedSize() override { return 0; }
};
int TypefaceEmpty::OnGetTableTags(FontTableTag *) const { return 0; }
size_t TypefaceEmpty::OnGetTableData(FontTableTag, size_t, size_t,
                                     void *) const {
  return 0;
}
void TypefaceEmpty::OnCharsToGlyphs(const uint32_t *, int, GlyphID *) const {}
std::shared_ptr<Data> TypefaceEmpty::OnGetData() { return nullptr; }
uint32_t TypefaceEmpty::OnGetUPEM() const { return 0; }
bool TypefaceEmpty::OnContainsColorTable() const { return false; }
std::unique_ptr<ScalerContext> TypefaceEmpty::OnCreateScalerContext(
    const ScalerContextDesc *desc) const {
  return std::unique_ptr<ScalerContextEmpty>();
}

VariationPosition TypefaceEmpty::OnGetVariationDesignPosition() const {
  return VariationPosition();
}

std::vector<VariationAxis> TypefaceEmpty::OnGetVariationDesignParameters()
    const {
  return {};
}

std::shared_ptr<Typeface> TypefaceEmpty::OnMakeVariation(
    const FontArguments &args) const {
  return nullptr;
}

}  // namespace skity
