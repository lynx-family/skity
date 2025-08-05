// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
#include <cstring>
#include <skity/text/font.hpp>
#include <skity/text/text_blob.hpp>
#include <skity/text/text_run.hpp>
#include <skity/text/typeface.hpp>
#include <skity/text/utf.hpp>

namespace skity {

Vec2 TextBlob::GetBoundSize() const {
  auto rect = GetBoundsRect();
  return Vec2{rect.Width(), rect.Height()};
}

Rect TextBlob::GetBoundsRect() const {
  if (text_run_.empty()) {
    return Rect::MakeEmpty();
  }

  float top = std::numeric_limits<float>::max();
  float bottom = std::numeric_limits<float>::min();
  float left = std::numeric_limits<float>::max();
  float right = std::numeric_limits<float>::min();
  float advance_x = 0;
  for (auto const &run : text_run_) {
    auto typeface = run.LockTypeface();
    Font font(typeface, run.GetFontSize());
    auto const &glyph_ids = run.GetGlyphInfo();
    std::vector<const GlyphData *> glyphs(glyph_ids.size());
    Paint paint;
    paint.SetTextSize(run.GetFontSize());
    font.LoadGlyphMetrics(glyph_ids.data(), glyph_ids.size(), glyphs.data(),
                          paint);
    if (glyphs.empty()) {
      continue;
    }

    std::vector<float> pos_y;
    std::vector<float> pos_x;
    if (run.GetPosX().empty()) {
      pos_x.reserve(glyphs.size());
      pos_y.reserve(glyphs.size());
      for (auto const &glyph : glyphs) {
        pos_x.emplace_back(advance_x);
        pos_y.emplace_back(0);
        advance_x += glyph->AdvanceX();
      }
    } else {
      pos_y.resize(run.GetPosX().size(), 0);
      for (size_t i = 0; i < pos_y.size(); ++i) {
        if (i < run.GetPosY().size()) {
          pos_y[i] += run.GetPosY()[i];
        }
      }

      for (auto rx : run.GetPosX()) {
        pos_x.emplace_back(rx);
      }
    }

    for (size_t i = 0; i < glyphs.size(); ++i) {
      auto const &glyph = glyphs[i];
      float x = pos_x[i];
      float y = pos_y[i];
      top = std::min(top, y - glyph->GetYMax());
      bottom = std::max(bottom, y - glyph->GetYMin());
      left = std::min(left, x + glyph->GetHoriBearingX());
      right = std::max(right, x + glyph->GetHoriBearingX() + glyph->GetWidth());
    }
  }
  return Rect::MakeLTRB(left, top, right, bottom);
}

// static
Rect TextBlob::ComputeBounds(uint32_t count, const GlyphID *glyphs,
                             const float *position_x, const float *position_y,
                             const Font &font, const Paint &paint) {
  std::vector<const GlyphData *> glyph_info(count);
  font.LoadGlyphMetrics(glyphs, count, glyph_info.data(), paint);

  float left = position_x[0];
  float right = position_x[count - 1] +
                glyph_info[count - 1]->GetHoriBearingX() +
                glyph_info[count - 1]->GetWidth();
  float top = std::numeric_limits<float>::max();
  float bottom = std::numeric_limits<float>::min();
  for (size_t i = 0; i < count; ++i) {
    top = std::min(top, position_y[i] - glyph_info[i]->GetHoriBearingY());
    bottom = std::max(bottom, position_y[i] - glyph_info[i]->GetYMin());
  }

  float stroke_width =
      paint.GetStyle() != Paint::kFill_Style ? paint.GetStrokeWidth() : 0;
  return Rect(std::floor(left - stroke_width), std::floor(top - stroke_width),
              std::floor(right + stroke_width),
              std::floor(bottom + stroke_width));
}

class SimpleDelegate : public TypefaceDelegate {
 public:
  explicit SimpleDelegate(
      const std::vector<std::shared_ptr<Typeface>> &typeface)
      : typefaces_(typeface) {}

  ~SimpleDelegate() override = default;

  std::shared_ptr<Typeface> Fallback(Unichar code_point,
                                     Paint const &) override {
    for (auto const &typeface : typefaces_) {
      if (typeface->ContainGlyph(code_point)) {
        return typeface;
      }
    }

    return nullptr;
  }

  std::vector<std::vector<Unichar>> BreakTextRun(const char *) override {
    return {};
  }

 private:
  std::vector<std::shared_ptr<Typeface>> typefaces_;
};

std::unique_ptr<TypefaceDelegate>
TypefaceDelegate::CreateSimpleFallbackDelegate(
    const std::vector<std::shared_ptr<Typeface>> &typefaces) {
  if (typefaces.empty()) {
    return {};
  }

  return std::make_unique<SimpleDelegate>(typefaces);
}

std::shared_ptr<TextBlob> TextBlobBuilder::BuildTextBlob(
    const char *text, const Paint &paint, TypefaceDelegate *delegate) {
  if (!paint.GetTypeface()) {
    return nullptr;
  }

  if (delegate) {
    return GenerateBlobWithDelegate(text, paint, delegate);
  } else {
    return GenerateBlobWithoutDelegate(text, paint);
  }
}

std::shared_ptr<TextBlob> TextBlobBuilder::BuildTextBlob(
    const std::string &text, const Paint &paint) {
  return BuildTextBlob(text.c_str(), paint, nullptr);
}

std::shared_ptr<TextBlob> TextBlobBuilder::GenerateBlobWithDelegate(
    const char *text, Paint const &paint, TypefaceDelegate *delegate) {
  auto typeface = paint.GetTypeface();

  auto break_result = delegate->BreakTextRun(text);

  if (break_result.empty()) {
    std::vector<Unichar> code_points = {};
    UTF::UTF8ToCodePoint(text, std::strlen(text), code_points);

    auto runs = GenerateTextRuns(code_points, typeface, paint, delegate);

    return std::make_shared<TextBlob>(runs);
  } else {
    return GenerateBlobWithMultiRun(break_result, paint, delegate);
  }
}

std::shared_ptr<TextBlob> TextBlobBuilder::GenerateBlobWithoutDelegate(
    const char *text, Paint const &paint) {
  auto typeface = paint.GetTypeface();

  std::vector<Unichar> code_points = {};

  UTF::UTF8ToCodePoint(text, std::strlen(text), code_points);

  if (code_points.empty()) {
    return nullptr;
  }

  std::vector<TextRun> runs = {};

  runs.emplace_back(GenerateTextRun(code_points, typeface, paint.GetTextSize(),
                                    paint.GetStyle() != Paint::kFill_Style));

  return std::make_shared<TextBlob>(runs);
}

std::shared_ptr<TextBlob> TextBlobBuilder::GenerateBlobWithMultiRun(
    std::vector<std::vector<Unichar>> const &code_points, Paint const &paint,
    TypefaceDelegate *delegate) {
  auto typeface = paint.GetTypeface();

  std::vector<TextRun> runs = {};

  for (auto const &cps : code_points) {
    auto sub_runs = GenerateTextRuns(cps, typeface, paint, delegate);
    runs.insert(runs.end(), sub_runs.begin(), sub_runs.end());
  }

  return std::make_shared<TextBlob>(runs);
}

std::vector<TextRun> TextBlobBuilder::GenerateTextRuns(
    std::vector<Unichar> const &code_points, std::shared_ptr<Typeface> typeface,
    Paint const &paint, TypefaceDelegate *delegate) {
  float font_size = paint.GetTextSize();
  std::vector<TextRun> runs = {};

  auto prev_char_typeface = typeface;
  const auto default_typeface = typeface;
  std::vector<GlyphID> infos = {};
  for (auto cp : code_points) {
    auto glyph_id = default_typeface->UnicharToGlyph(cp);
    if (glyph_id != 0) {
      if (prev_char_typeface != default_typeface) {
        // need to create a new TextRun
        Font font{prev_char_typeface, font_size};
        runs.emplace_back(TextRun(font, infos));
        // begin new TextRun
        infos.clear();
      }
      infos.emplace_back(glyph_id);
      prev_char_typeface = default_typeface;
      continue;
    }

    // fallback to base typeface
    auto fallback_typeface = delegate->Fallback(cp, paint);
    if (!fallback_typeface) {
      // failed fallback
      continue;
    }
    glyph_id = fallback_typeface->UnicharToGlyph(cp);
    if (glyph_id != 0) {
      if (prev_char_typeface != fallback_typeface) {
        // need to create a new TextRun
        Font font{prev_char_typeface, font_size};
        runs.emplace_back(TextRun(font, infos));
        // begin new TextRun
        infos.clear();
      }
      infos.emplace_back(glyph_id);
      prev_char_typeface = fallback_typeface;
      continue;
    }
  }
  if (!infos.empty()) {
    Font font{prev_char_typeface, font_size};
    runs.emplace_back(TextRun(font, std::move(infos)));
  }

  return runs;
}

TextRun TextBlobBuilder::GenerateTextRun(
    std::vector<Unichar> const &code_points, std::shared_ptr<Typeface> typeface,
    float font_size, bool) {
  std::vector<GlyphID> infos = {};

  for (Unichar cp : code_points) {
    // TODO(tangruiwen) maybe check if need glyph path
    auto glyph_id = typeface->UnicharToGlyph(cp);
    infos.emplace_back(glyph_id);
  }

  Font f{typeface, font_size};
  return TextRun{f, infos};
}

}  // namespace skity
