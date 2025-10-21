/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <glm/gtc/matrix_transform.hpp>
#include <skity/text/font_manager.hpp>

#include "src/text/scaler_context.hpp"
#include "src/text/scaler_context_desc.hpp"

namespace skity {

std::shared_ptr<Typeface> Typeface::GetDefaultTypeface(
    class FontStyle font_style) {
  return FontManager::RefDefault()->GetDefaultTypeface(font_style);
}

std::shared_ptr<Typeface> Typeface::MakeFromData(
    const std::shared_ptr<Data>& data) {
  if (!data) {
    return nullptr;
  }
  return FontManager::RefDefault()->MakeFromData(data);
}

std::shared_ptr<Typeface> Typeface::MakeFromFile(const char* path) {
  return FontManager::RefDefault()->MakeFromFile(path);
}

bool Typeface::ContainGlyph(Unichar code_point) const {
  return this->UnicharToGlyph(code_point);
}

void Typeface::UnicharsToGlyphs(const uint32_t uni[], int count,
                                GlyphID glyphs[]) const {
  if (count > 0 && glyphs && uni) {
    this->OnCharsToGlyphs(uni, count, glyphs);
  }
}

GlyphID Typeface::UnicharToGlyph(uint32_t unichar) const {
  GlyphID glyphs[1] = {0};
  this->OnCharsToGlyphs(&unichar, 1, glyphs);
  return glyphs[0];
}

int Typeface::CountTables() const { return this->OnGetTableTags(nullptr); }

int Typeface::GetTableTags(FontTableTag tags[]) const {
  return this->OnGetTableTags(tags);
}

size_t Typeface::GetTableSize(FontTableTag tag) const {
  return this->OnGetTableData(tag, 0, ~0U, nullptr);
}

size_t Typeface::GetTableData(FontTableTag tag, size_t offset, size_t length,
                              void* data) const {
  return this->OnGetTableData(tag, offset, length, data);
}
uint32_t Typeface::GetUnitsPerEm() const { return this->OnGetUPEM(); }

bool Typeface::ContainsColorTable() const {
  return this->OnContainsColorTable();
}

std::unique_ptr<ScalerContext> Typeface::CreateScalerContext(
    const ScalerContextDesc* desc) const {
  return this->OnCreateScalerContext(desc);
}

VariationPosition Typeface::GetVariationDesignPosition() const {
  return OnGetVariationDesignPosition();
}
std::vector<VariationAxis> Typeface::GetVariationDesignParameters() const {
  return OnGetVariationDesignParameters();
}

std::shared_ptr<Typeface> Typeface::MakeVariation(
    const FontArguments& args) const {
  return OnMakeVariation(args);
}

FontDescriptor Typeface::GetFontDescriptor() const {
  FontDescriptor desc;

  OnGetFontDescriptor(desc);

  desc.style = GetFontStyle();

  desc.variation_position = GetVariationDesignPosition();

  return desc;
}

}  // namespace skity
