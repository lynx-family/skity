// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_TEXT_RUN_HPP
#define INCLUDE_SKITY_TEXT_TEXT_RUN_HPP

#include <skity/graphic/path.hpp>
#include <skity/macros.hpp>
#include <skity/text/font.hpp>
#include <skity/text/glyph.hpp>
#include <vector>

namespace skity {

class Typeface;

/**
 * Simple class for represents a sequence of characters that share a single
 * property set.
 * Like `Typeface`, `FontSize` ...
 *
 */
class SKITY_API TextRun final {
 public:
  TextRun(const Font& font, std::vector<GlyphID> info);
  TextRun(const Font& font, std::vector<GlyphID> info,
          std::vector<float> pos_x);
  TextRun(const Font& font, std::vector<GlyphID> info, std::vector<float> pos_x,
          std::vector<float> pos_y);

  TextRun(const TextRun&) = default;

  ~TextRun();

  TextRun& operator=(const TextRun&) = default;

  std::vector<GlyphID> const& GetGlyphInfo() const;

  std::vector<float> const& GetPosX() const;

  std::vector<float> const& GetPosY() const;

  std::shared_ptr<Typeface> LockTypeface() const { return font_.GetTypeface(); }

  float GetFontSize() const { return font_.GetSize(); }

  const Font& GetFont() const { return font_; }

 private:
  Font font_ = {};
  std::vector<GlyphID> glyph_info_;
  std::vector<float> pos_x_;
  std::vector<float> pos_y_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_TEXT_RUN_HPP
