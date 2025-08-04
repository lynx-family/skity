// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/scaler_context.hpp"

namespace skity {

ScalerContext::ScalerContext(std::shared_ptr<Typeface> typeface,
                             const ScalerContextDesc* desc)
    : typeface_(std::move(typeface)), desc_(*desc) {}

void ScalerContext::MakeGlyph(GlyphData* glyph_data) {
  this->GenerateMetrics(glyph_data);
}
void ScalerContext::GetImage(GlyphData* glyph, const StrokeDesc& stroke_desc) {
  this->GenerateImage(glyph, stroke_desc);
}

void ScalerContext::GetImageInfo(GlyphData* glyph,
                                 const StrokeDesc& stroke_desc) {
  this->GenerateImageInfo(glyph, stroke_desc);
}

void ScalerContext::GetPath(GlyphData* glyph) { this->GeneratePath(glyph); }
void ScalerContext::GetFontMetrics(FontMetrics* metrics) {
  this->GenerateFontMetrics(metrics);
}
}  // namespace skity
