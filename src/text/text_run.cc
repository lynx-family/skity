// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/text/text_run.hpp>
#include <skity/text/typeface.hpp>

namespace skity {

TextRun::TextRun(const Font& font, std::vector<GlyphID> info)
    : font_(font), glyph_info_(std::move(info)) {}

TextRun::TextRun(const Font& font, std::vector<GlyphID> info,
                 std::vector<float> pos_x)
    : font_(font), glyph_info_(std::move(info)), pos_x_(std::move(pos_x)) {}

TextRun::TextRun(const Font& font, std::vector<GlyphID> info,
                 std::vector<float> pos_x, std::vector<float> pos_y)
    : font_(font),
      glyph_info_(std::move(info)),
      pos_x_(std::move(pos_x)),
      pos_y_(std::move(pos_y)) {}

TextRun::~TextRun() = default;

std::vector<GlyphID> const& TextRun::GetGlyphInfo() const {
  return glyph_info_;
}

std::vector<float> const& TextRun::GetPosX() const { return pos_x_; }

std::vector<float> const& TextRun::GetPosY() const { return pos_y_; }

}  // namespace skity
