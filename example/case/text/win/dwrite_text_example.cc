// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <skity/skity.hpp>
#include <skity/utils/settings.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "common/app.hpp"

namespace {

constexpr float kWindowWidth = 2800.f;
constexpr float kWindowHeight = 1880.f;
constexpr bool kEnableDWriteFontManager = true;

skity::Color Color(uint8_t r, uint8_t g, uint8_t b) {
  return skity::ColorSetARGB(0xFF, r, g, b);
}

skity::FontStyle MakeStyle(int weight, skity::FontStyle::Slant slant =
                                           skity::FontStyle::kUpright_Slant) {
  return skity::FontStyle{weight, skity::FontStyle::kNormal_Width, slant};
}

void AppendUtf8(std::string* text, uint32_t cp) {
  if (cp <= 0x7F) {
    text->push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    text->push_back(static_cast<char>(0xC0 | (cp >> 6)));
    text->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    text->push_back(static_cast<char>(0xE0 | (cp >> 12)));
    text->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    text->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    text->push_back(static_cast<char>(0xF0 | (cp >> 18)));
    text->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    text->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    text->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

std::string Utf8(std::initializer_list<uint32_t> cps) {
  std::string text;
  for (auto cp : cps) {
    AppendUtf8(&text, cp);
  }
  return text;
}

std::string AppendUtf8Text(std::string text,
                           std::initializer_list<uint32_t> cps) {
  for (auto cp : cps) {
    AppendUtf8(&text, cp);
  }
  return text;
}

std::vector<skity::Unichar> Codepoints(const char* text) {
  std::vector<skity::Unichar> cps;
  skity::UTF::UTF8ToCodePoint(text, std::strlen(text), cps);
  return cps;
}

std::string SlantName(skity::FontStyle::Slant slant) {
  switch (slant) {
    case skity::FontStyle::kUpright_Slant:
      return "upright";
    case skity::FontStyle::kItalic_Slant:
      return "italic";
    case skity::FontStyle::kOblique_Slant:
      return "oblique";
  }
  return "unknown";
}

std::string StyleSummary(skity::FontStyle style) {
  std::ostringstream stream;
  stream << "w" << style.weight() << " width" << style.width() << " "
         << SlantName(style.slant());
  return stream.str();
}

std::string TypefaceSummary(const std::shared_ptr<skity::Typeface>& typeface) {
  if (!typeface) {
    return "null typeface";
  }

  auto desc = typeface->GetFontDescriptor();
  std::ostringstream stream;
  stream << (desc.family_name.empty() ? "(no family)" : desc.family_name);
  if (!desc.full_name.empty() && desc.full_name != desc.family_name) {
    stream << " / " << desc.full_name;
  }
  stream << " | " << StyleSummary(typeface->GetFontStyle());
  stream << " | tables=" << typeface->CountTables();
  stream << " | color=" << (typeface->ContainsColorTable() ? "yes" : "no");
  return stream.str();
}

std::string AxisSummary(const std::shared_ptr<skity::Typeface>& typeface) {
  if (!typeface) {
    return "axes=0";
  }

  std::ostringstream stream;
  stream << "axes=" << typeface->GetVariationDesignParameters().size();
  stream << " pos="
         << typeface->GetVariationDesignPosition().GetCoordinates().size();
  return stream.str();
}

std::shared_ptr<skity::Typeface> MatchOrDefault(
    const std::shared_ptr<skity::FontManager>& font_manager, const char* family,
    skity::FontStyle style) {
  auto typeface = font_manager->MatchFamilyStyle(family, style);
  if (typeface) {
    return typeface;
  }
  return skity::Typeface::GetDefaultTypeface(style);
}

void AddTypeface(std::vector<std::shared_ptr<skity::Typeface>>* typefaces,
                 std::shared_ptr<skity::Typeface> typeface) {
  if (!typeface) {
    return;
  }
  auto same_typeface =
      [&typeface](const std::shared_ptr<skity::Typeface>& item) {
        return item && item->TypefaceId() == typeface->TypefaceId();
      };
  if (std::find_if(typefaces->begin(), typefaces->end(), same_typeface) ==
      typefaces->end()) {
    typefaces->push_back(std::move(typeface));
  }
}

std::shared_ptr<skity::Typeface> MakeVariation(
    const std::shared_ptr<skity::Typeface>& typeface,
    std::initializer_list<std::pair<skity::FourByteTag, float>> coordinates) {
  if (!typeface) {
    return nullptr;
  }

  skity::VariationPosition position;
  for (const auto& coordinate : coordinates) {
    position.AddCoordinate(coordinate.first, coordinate.second);
  }

  skity::FontArguments arguments;
  arguments.SetVariationDesignPosition(position);
  auto variation = typeface->MakeVariation(arguments);
  return variation ? variation : typeface;
}

std::shared_ptr<skity::Typeface> MakeDataTypeface(
    const std::shared_ptr<skity::FontManager>& font_manager, const char* path,
    int ttc_index = 0) {
  auto data = skity::Data::MakeFromFileName(path);
  if (!data) {
    return nullptr;
  }
  return font_manager->MakeFromData(data, ttc_index);
}

std::shared_ptr<skity::Typeface> MatchCharacter(
    const std::shared_ptr<skity::FontManager>& font_manager,
    skity::Unichar codepoint, const char* family = nullptr,
    const char* bcp47 = nullptr) {
  const char* bcp47_list[] = {bcp47};
  const char** bcp47_ptr = bcp47 == nullptr ? nullptr : bcp47_list;
  const int bcp47_count = bcp47 == nullptr ? 0 : 1;
  return font_manager->MatchFamilyStyleCharacter(
      family, skity::FontStyle::Normal(), bcp47_ptr, bcp47_count, codepoint);
}

std::unique_ptr<skity::TypefaceDelegate> BuildFallbackDelegate(
    const std::shared_ptr<skity::FontManager>& font_manager,
    const std::shared_ptr<skity::Typeface>& base_typeface) {
  std::vector<std::shared_ptr<skity::Typeface>> typefaces;
  AddTypeface(&typefaces, base_typeface);
  AddTypeface(&typefaces, MatchOrDefault(font_manager, "Segoe UI",
                                         skity::FontStyle::Normal()));
  AddTypeface(&typefaces,
              MatchCharacter(font_manager, 0x4E2D, nullptr, "zh-CN"));
  AddTypeface(&typefaces,
              MatchCharacter(font_manager, 0x304B, nullptr, "ja-JP"));
  AddTypeface(&typefaces,
              MatchCharacter(font_manager, 0xD55C, nullptr, "ko-KR"));
  AddTypeface(&typefaces,
              MatchCharacter(font_manager, 0x0639, nullptr, "ar-SA"));
  AddTypeface(&typefaces,
              MatchCharacter(font_manager, 0x0939, nullptr, "hi-IN"));
  AddTypeface(&typefaces, MatchCharacter(font_manager, 0x1F600));
  AddTypeface(&typefaces, skity::Typeface::MakeFromFile(EXAMPLE_IMAGE_ROOT
                                                        "/NotoColorEmoji.ttf"));
  AddTypeface(&typefaces, skity::Typeface::MakeFromFile(
                              EXAMPLE_IMAGE_ROOT "/NotoEmoji-Regular.ttf"));
  return skity::TypefaceDelegate::CreateSimpleFallbackDelegate(typefaces);
}

skity::Paint PaintForText(
    std::shared_ptr<skity::Typeface> typeface, float text_size,
    skity::Color color, skity::Paint::Style style = skity::Paint::kFill_Style) {
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetTypeface(std::move(typeface));
  paint.SetTextSize(text_size);
  paint.SetColor(color);
  paint.SetFillColor(color);
  paint.SetStrokeColor(color);
  paint.SetStyle(style);
  return paint;
}

void DrawText(skity::Canvas* canvas, const std::string& text, float x, float y,
              const skity::Paint& paint) {
  canvas->DrawSimpleText(text.c_str(), x, y, paint);
}

void DrawBlobText(skity::Canvas* canvas, const std::string& text, float x,
                  float y, const skity::Paint& paint,
                  skity::TypefaceDelegate* delegate) {
  skity::TextBlobBuilder builder;
  auto blob = builder.BuildTextBlob(text.c_str(), paint, delegate);
  canvas->DrawTextBlob(blob.get(), x, y, paint);
}

void DrawGlyphText(skity::Canvas* canvas, const std::string& text, float x,
                   float y, const skity::Font& font,
                   const skity::Paint& paint) {
  auto cps = Codepoints(text.c_str());
  if (cps.empty() || !font.GetTypeface()) {
    return;
  }

  std::vector<skity::GlyphID> glyphs(cps.size());
  font.GetTypeface()->UnicharsToGlyphs(cps.data(), static_cast<int>(cps.size()),
                                       glyphs.data());

  std::vector<const skity::GlyphData*> glyph_data(glyphs.size());
  font.LoadGlyphMetrics(glyphs.data(), static_cast<uint32_t>(glyphs.size()),
                        glyph_data.data(), paint);

  std::vector<float> pos_x;
  std::vector<float> pos_y;
  pos_x.reserve(glyphs.size());
  pos_y.reserve(glyphs.size());
  float advance_x = x;
  for (auto glyph : glyph_data) {
    pos_x.push_back(advance_x);
    pos_y.push_back(y);
    advance_x += glyph == nullptr ? 0.f : glyph->AdvanceX();
  }

  canvas->DrawGlyphs(static_cast<int>(glyphs.size()), glyphs.data(),
                     pos_x.data(), pos_y.data(), font, paint);
}

void DrawLabel(skity::Canvas* canvas, const std::string& text, float x, float y,
               float width, std::shared_ptr<skity::Typeface> label_typeface) {
  skity::Paint paint =
      PaintForText(std::move(label_typeface), 19.f, Color(72, 82, 96));
  const size_t max_chars = static_cast<size_t>(std::max(24.f, width / 10.2f));
  const auto trim_left = [](std::string line) {
    while (!line.empty() && line.front() == ' ') {
      line.erase(line.begin());
    }
    return line;
  };
  auto wrap_line = [max_chars](const std::string& line) {
    if (line.size() <= max_chars) {
      return std::make_pair(line, std::string());
    }

    const size_t preferred = line.rfind(' ', max_chars);
    const size_t split = preferred == std::string::npos || preferred < 12
                             ? max_chars
                             : preferred;
    return std::make_pair(line.substr(0, split), line.substr(split));
  };

  auto lines = wrap_line(text);
  std::string second = trim_left(lines.second);
  if (second.size() > max_chars) {
    second = second.substr(0, max_chars - 3) + "...";
  }

  DrawText(canvas, lines.first, x, y, paint);
  if (!second.empty()) {
    DrawText(canvas, second, x, y + 24.f, paint);
  }
}

void DrawSectionTitle(skity::Canvas* canvas, const std::string& title, float x,
                      float y,
                      const std::shared_ptr<skity::Typeface>& typeface) {
  auto paint = PaintForText(typeface, 30.f, Color(28, 38, 50));
  DrawText(canvas, title, x, y, paint);
}

void DrawDivider(skity::Canvas* canvas, float x, float y, float width) {
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(Color(205, 213, 224));
  paint.SetStrokeColor(Color(205, 213, 224));
  paint.SetStrokeWidth(1.f);
  canvas->DrawLine(x, y, x + width, y, paint);
}

struct FamilyCase {
  const char* family;
  skity::FontStyle style;
  std::string sample;
};

void DrawFamilyCases(skity::Canvas* canvas,
                     const std::shared_ptr<skity::FontManager>& font_manager,
                     const std::shared_ptr<skity::Typeface>& label_typeface,
                     float x, float y, float column_width) {
  DrawSectionTitle(canvas, "Family and style matching", x, y, label_typeface);
  y += 28.f;
  DrawDivider(canvas, x, y, column_width);
  y += 28.f;

  const std::vector<FamilyCase> cases = {
      {"Arial", skity::FontStyle::Normal(), "Arial normal"},
      {"Arial", skity::FontStyle::Bold(), "Arial bold"},
      {"Arial", skity::FontStyle::Italic(), "Arial italic"},
      {"Arial", skity::FontStyle::BoldItalic(), "Arial bold italic"},
      {"Times New Roman", skity::FontStyle::BoldItalic(), "Serif bold italic"},
      {"Consolas", skity::FontStyle::Italic(), "Consolas italic 012345"},
      {"Segoe UI", MakeStyle(skity::FontStyle::kSemiBold_Weight),
       "Segoe UI semibold"},
      {"Microsoft YaHei", skity::FontStyle::Bold(),
       AppendUtf8Text("Microsoft YaHei ", {0x4E2D, 0x6587})},
      {"SimSun", skity::FontStyle::Normal(),
       AppendUtf8Text("SimSun ", {0x5B8B, 0x4F53})},
      {"Segoe UI Emoji", skity::FontStyle::Normal(),
       AppendUtf8Text("Segoe UI Emoji ", {0x1F600, 0x1F680})},
      {"SkityMissingFamilyName", skity::FontStyle::Bold(),
       "Missing family fallback"},
  };

  for (const auto& family_case : cases) {
    auto typeface =
        MatchOrDefault(font_manager, family_case.family, family_case.style);
    auto paint = PaintForText(typeface, 32.f, Color(20, 25, 33));
    DrawText(canvas, family_case.sample, x, y, paint);
    DrawLabel(
        canvas,
        std::string(family_case.family) + " -> " + TypefaceSummary(typeface), x,
        y + 28.f, column_width, label_typeface);
    y += 82.f;
  }
}

void DrawWeightSweep(skity::Canvas* canvas,
                     const std::shared_ptr<skity::FontManager>& font_manager,
                     const std::shared_ptr<skity::Typeface>& label_typeface,
                     float x, float y, float column_width) {
  DrawSectionTitle(canvas, "Weight selection sweep", x, y, label_typeface);
  y += 28.f;
  DrawDivider(canvas, x, y, column_width);
  y += 28.f;

  const int weights[] = {100, 200, 300, 400, 500, 600, 700, 800, 900};
  for (auto weight : weights) {
    auto style = MakeStyle(weight);
    auto typeface = MatchOrDefault(font_manager, "Segoe UI", style);
    auto paint = PaintForText(typeface, 31.f, Color(18, 65, 95));
    std::ostringstream sample;
    sample << "Segoe UI weight " << weight << " AaBbCc 123";
    DrawText(canvas, sample.str(), x, y, paint);
    DrawLabel(canvas, TypefaceSummary(typeface), x, y + 28.f, column_width,
              label_typeface);
    y += 68.f;
  }

  y += 8.f;
  for (auto weight : {300, 400, 700, 900}) {
    auto style = MakeStyle(weight);
    auto typeface = MatchOrDefault(font_manager, "Microsoft YaHei", style);
    auto paint = PaintForText(typeface, 31.f, Color(40, 83, 45));
    auto sample = AppendUtf8Text("YaHei weight ", {0x4E2D, 0x6587, 0x95E8});
    DrawText(canvas, sample + " " + std::to_string(weight), x, y, paint);
    DrawLabel(canvas, TypefaceSummary(typeface), x, y + 28.f, column_width,
              label_typeface);
    y += 68.f;
  }
}

void DrawVariationCases(skity::Canvas* canvas,
                        const std::shared_ptr<skity::FontManager>& font_manager,
                        const std::shared_ptr<skity::Typeface>& label_typeface,
                        float x, float y, float column_width) {
  DrawSectionTitle(canvas, "Variable font probes", x, y, label_typeface);
  y += 28.f;
  DrawDivider(canvas, x, y, column_width);
  y += 30.f;

  auto flex =
      font_manager->MakeFromFile(TEST_FONT_ROOT "/RobotoFlex-Regular.ttf");
  auto flex_light =
      MakeVariation(flex, {{skity::SetFourByteTag('w', 'g', 'h', 't'), 100.f}});
  auto flex_bold =
      MakeVariation(flex, {{skity::SetFourByteTag('w', 'g', 'h', 't'), 900.f}});
  auto flex_multi = MakeVariation(
      flex, {
                {skity::SetFourByteTag('w', 'g', 'h', 't'), 700.f},
                {skity::SetFourByteTag('w', 'd', 't', 'h'), 125.f},
                {skity::SetFourByteTag('o', 'p', 's', 'z'), 72.f},
                {skity::SetFourByteTag('G', 'R', 'A', 'D'), 50.f},
            });
  auto flex_data =
      MakeDataTypeface(font_manager, TEST_FONT_ROOT "/RobotoFlex-Regular.ttf");
  flex_data = MakeVariation(
      flex_data, {{skity::SetFourByteTag('w', 'g', 'h', 't'), 700.f}});

  const std::vector<std::pair<std::string, std::shared_ptr<skity::Typeface>>>
      cases = {
          {"RobotoFlex default AaBbCc123", flex},
          {"RobotoFlex wght100 AaBbCc123", flex_light},
          {"RobotoFlex wght900 AaBbCc123", flex_bold},
          {"RobotoFlex multi-axis AaBbCc123", flex_multi},
          {"RobotoFlex data wght700 AaBbCc123", flex_data},
      };

  for (const auto& item : cases) {
    auto paint = PaintForText(item.second, 32.f, Color(35, 66, 102));
    DrawText(canvas, item.first, x, y, paint);
    DrawLabel(canvas,
              AxisSummary(item.second) + " | " + TypefaceSummary(item.second),
              x, y + 28.f, column_width, label_typeface);
    y += 84.f;
  }
}

void DrawCollectionAndDataCases(
    skity::Canvas* canvas,
    const std::shared_ptr<skity::FontManager>& font_manager,
    const std::shared_ptr<skity::Typeface>& label_typeface, float x, float y,
    float column_width) {
  DrawSectionTitle(canvas, "Collection and data fonts", x, y, label_typeface);
  y += 28.f;
  DrawDivider(canvas, x, y, column_width);
  y += 30.f;

  auto cjk_index0 =
      font_manager->MakeFromFile(TEST_FONT_ROOT "/NotoSansCJK-Regular.ttc", 0);
  auto cjk_index1 =
      font_manager->MakeFromFile(TEST_FONT_ROOT "/NotoSansCJK-Regular.ttc", 1);
  auto cjk_index2 =
      font_manager->MakeFromFile(TEST_FONT_ROOT "/NotoSansCJK-Regular.ttc", 2);
  auto cjk_index3 =
      font_manager->MakeFromFile(TEST_FONT_ROOT "/NotoSansCJK-Regular.ttc", 3);
  auto cjk_index4 =
      font_manager->MakeFromFile(TEST_FONT_ROOT "/NotoSansCJK-Regular.ttc", 4);
  auto emoji_data =
      MakeDataTypeface(font_manager, TEST_FONT_ROOT "/NotoColorEmoji.ttf");

  const std::vector<std::pair<std::string, std::shared_ptr<skity::Typeface>>>
      cases = {
          {AppendUtf8Text("TTC index0 JP U+95E8 ", {0x95E8, 0x20, 0x95E8}),
           cjk_index0},
          {AppendUtf8Text("TTC index1 KR U+95E8 ", {0x95E8, 0x20, 0x95E8}),
           cjk_index1},
          {AppendUtf8Text("TTC index2 SC U+95E8 ", {0x95E8, 0x20, 0x95E8}),
           cjk_index2},
          {AppendUtf8Text("TTC index3 TC U+95E8 ", {0x95E8, 0x20, 0x95E8}),
           cjk_index3},
          {AppendUtf8Text("TTC index4 HK U+95E8 ", {0x95E8, 0x20, 0x95E8}),
           cjk_index4},
          {Utf8({0x1F600, 0x20, 0x1F680, 0x20, 0x1F4A9}), emoji_data},
      };

  for (const auto& item : cases) {
    auto paint = PaintForText(item.second, 33.f, Color(59, 76, 45));
    DrawText(canvas, item.first, x, y, paint);
    DrawLabel(canvas, TypefaceSummary(item.second), x, y + 28.f, column_width,
              label_typeface);
    y += 80.f;
  }
}

void DrawFallbackAndEmoji(
    skity::Canvas* canvas,
    const std::shared_ptr<skity::FontManager>& font_manager,
    const std::shared_ptr<skity::Typeface>& label_typeface, float x, float y,
    float column_width) {
  DrawSectionTitle(canvas, "Fallback scripts and color glyphs", x, y,
                   label_typeface);
  y += 28.f;
  DrawDivider(canvas, x, y, column_width);
  y += 32.f;

  auto base_typeface =
      MatchOrDefault(font_manager, "Arial", skity::FontStyle::Normal());
  auto fallback_delegate = BuildFallbackDelegate(font_manager, base_typeface);

  auto mixed = AppendUtf8Text(
      "Fallback: ", {0x4E2D, 0x6587, 0x20, 0x65E5, 0x672C, 0x8A9E, 0x20, 0xD55C,
                     0xAD6D, 0xC5B4, 0x20, 0x1F600});
  std::string spacing_mixed = "Spacing: [A]   [";
  AppendUtf8(&spacing_mixed, 0x4E2D);
  spacing_mixed += "]   [";
  AppendUtf8(&spacing_mixed, 0x1F600);
  spacing_mixed += "]   [B]";
  auto complex_mixed = AppendUtf8Text(
      "Arabic/Hindi: ", {0x0639, 0x0631, 0x0628, 0x064A, 0x20, 0x0939, 0x093F,
                         0x0928, 0x094D, 0x0926, 0x0940});
  auto paint = PaintForText(base_typeface, 34.f, Color(27, 49, 79));
  DrawBlobText(canvas, mixed, x, y, paint, fallback_delegate.get());
  y += 48.f;
  DrawBlobText(canvas, spacing_mixed, x, y, paint, fallback_delegate.get());
  y += 48.f;
  DrawBlobText(canvas, complex_mixed, x, y, paint, fallback_delegate.get());
  DrawLabel(canvas,
            "TextBlob fallback from Arial, with visible inter-run spaces", x,
            y + 34.f, column_width, label_typeface);
  y += 114.f;

  auto emoji_text = Utf8({0x1F600, 0x20, 0x1F642, 0x20, 0x1F680, 0x20, 0x1F4A9,
                          0x20, 0x1F469, 0x200D, 0x1F4BB});
  auto short_emoji_text = Utf8({0x1F600, 0x20, 0x1F680, 0x20, 0x1F4A9});
  auto emoji_system = MatchOrDefault(font_manager, "Segoe UI Emoji",
                                     skity::FontStyle::Normal());
  auto emoji_file =
      skity::Typeface::MakeFromFile(EXAMPLE_IMAGE_ROOT "/NotoColorEmoji.ttf");
  auto emoji_mono = skity::Typeface::MakeFromFile(EXAMPLE_IMAGE_ROOT
                                                  "/NotoEmoji-Regular.ttf");
  struct EmojiCase {
    const char* label;
    std::string text;
    std::shared_ptr<skity::Typeface> typeface;
  };
  const std::vector<EmojiCase> emoji_cases = {
      {"system Segoe UI Emoji", emoji_text, emoji_system},
      {"file NotoColorEmoji.ttf", short_emoji_text, emoji_file},
      {"file NotoEmoji-Regular.ttf", short_emoji_text, emoji_mono},
  };
  for (const auto& item : emoji_cases) {
    auto name_paint = PaintForText(label_typeface, 28.f, Color(27, 49, 79));
    DrawText(canvas, item.label, x, y, name_paint);
    auto emoji_paint = PaintForText(item.typeface, 54.f, Color(23, 68, 66));
    DrawText(canvas, item.text, x, y + 56.f, emoji_paint);
    DrawLabel(canvas,
              std::string(item.label) + " | " + TypefaceSummary(item.typeface),
              x, y + 96.f, column_width, label_typeface);
    y += 144.f;
  }

  auto locale_cjk = MatchCharacter(font_manager, 0x95E8, "Arial", "zh-CN");
  auto locale_jp = MatchCharacter(font_manager, 0x304B, "Arial", "ja-JP");
  auto locale_kr = MatchCharacter(font_manager, 0xD55C, "Arial", "ko-KR");
  auto locale_ar = MatchCharacter(font_manager, 0x0639, "Arial", "ar-SA");
  auto locale_hi = MatchCharacter(font_manager, 0x0939, "Arial", "hi-IN");
  const std::vector<std::pair<std::string, std::shared_ptr<skity::Typeface>>>
      locale_cases = {
          {"zh-CN U+95E8", locale_cjk}, {"ja-JP U+304B", locale_jp},
          {"ko-KR U+D55C", locale_kr},  {"ar-SA U+0639", locale_ar},
          {"hi-IN U+0939", locale_hi},
      };
  for (const auto& item : locale_cases) {
    DrawLabel(canvas, item.first + " -> " + TypefaceSummary(item.second), x, y,
              column_width, label_typeface);
    y += 46.f;
  }
}

void DrawPaintModes(skity::Canvas* canvas,
                    const std::shared_ptr<skity::FontManager>& font_manager,
                    const std::shared_ptr<skity::Typeface>& label_typeface,
                    float x, float y, float column_width) {
  DrawSectionTitle(canvas, "Fill/stroke/fake-bold glyph masks", x, y,
                   label_typeface);
  y += 28.f;
  DrawDivider(canvas, x, y, column_width);
  y += 34.f;

  auto typeface =
      MatchOrDefault(font_manager, "Segoe UI", skity::FontStyle::Normal());
  const std::string sample = "DWrite Stroke Agj 123";

  struct PaintMode {
    const char* label;
    skity::Paint::Style style;
    float stroke_width;
    skity::Color fill_color;
    skity::Color stroke_color;
  };

  const std::vector<PaintMode> modes = {
      {"fill", skity::Paint::kFill_Style, 1.f, Color(21, 42, 77),
       Color(21, 42, 77)},
      {"stroke", skity::Paint::kStroke_Style, 1.8f, Color(21, 42, 77),
       Color(201, 48, 56)},
      {"stroke and fill", skity::Paint::kStrokeAndFill_Style, 2.2f,
       Color(20, 80, 52), Color(180, 70, 42)},
      {"stroke then fill", skity::Paint::kStrokeThenFill_Style, 2.6f,
       Color(22, 70, 118), Color(154, 65, 120)},
  };

  for (const auto& mode : modes) {
    auto paint = PaintForText(typeface, 45.f, mode.fill_color, mode.style);
    paint.SetStrokeWidth(mode.stroke_width);
    paint.SetFillColor(mode.fill_color);
    paint.SetStrokeColor(mode.stroke_color);
    DrawText(canvas, sample, x, y, paint);
    DrawLabel(canvas, mode.label, x, y + 38.f, column_width, label_typeface);
    y += 98.f;
  }

  skity::Font normal_font(typeface, 50.f);
  skity::Font fake_bold_font(typeface, 50.f);
  fake_bold_font.SetEmbolden(true);

  auto normal_paint = PaintForText(typeface, 50.f, Color(64, 64, 74),
                                   skity::Paint::kFill_Style);
  DrawGlyphText(canvas, "DrawGlyphs normal", x, y, normal_font, normal_paint);
  DrawLabel(canvas, "DrawGlyphs without embolden", x, y + 38.f, column_width,
            label_typeface);
  y += 100.f;

  auto bold_paint = PaintForText(typeface, 50.f, Color(82, 42, 86),
                                 skity::Paint::kFill_Style);
  DrawGlyphText(canvas, "DrawGlyphs fake bold", x, y, fake_bold_font,
                bold_paint);
  DrawLabel(canvas, "DrawGlyphs with Font::SetEmbolden(true)", x, y + 38.f,
            column_width, label_typeface);
  y += 100.f;

  auto bold_stroke_paint = PaintForText(typeface, 50.f, Color(95, 62, 24),
                                        skity::Paint::kStroke_Style);
  bold_stroke_paint.SetStrokeWidth(1.8f);
  bold_stroke_paint.SetStrokeColor(Color(95, 62, 24));
  DrawGlyphText(canvas, "Fake bold stroke", x, y, fake_bold_font,
                bold_stroke_paint);
  DrawLabel(canvas, "embolden plus stroke mask", x, y + 38.f, column_width,
            label_typeface);
}

void DrawTransforms(skity::Canvas* canvas,
                    const std::shared_ptr<skity::FontManager>& font_manager,
                    const std::shared_ptr<skity::Typeface>& label_typeface,
                    float x, float y, float column_width) {
  DrawSectionTitle(canvas, "Transforms and large path fallback", x, y,
                   label_typeface);
  y += 28.f;
  DrawDivider(canvas, x, y, column_width);
  y += 40.f;

  auto typeface =
      MatchOrDefault(font_manager, "Consolas", skity::FontStyle::Normal());
  auto paint = PaintForText(typeface, 45.f, Color(25, 72, 84));

  canvas->Save();
  canvas->Translate(x, y);
  canvas->Scale(1.25f, 1.25f);
  DrawText(canvas, "scaled text", 0.f, 0.f, paint);
  canvas->Restore();
  DrawLabel(canvas, "canvas scale 1.25x", x, y + 68.f, column_width,
            label_typeface);
  y += 128.f;

  canvas->Save();
  canvas->Translate(x, y);
  canvas->Skew(-0.25f, 0.f);
  DrawText(canvas, "skewed text", 0.f, 0.f, paint);
  canvas->Restore();
  DrawLabel(canvas, "canvas skew", x, y + 46.f, column_width, label_typeface);
  y += 116.f;

  canvas->Save();
  canvas->Rotate(-9.f, x + 160.f, y);
  DrawText(canvas, "rotated text", x, y, paint);
  canvas->Restore();
  DrawLabel(canvas, "canvas rotate around baseline", x, y + 54.f, column_width,
            label_typeface);
  y += 128.f;

  auto large_paint = PaintForText(typeface, 122.f, Color(65, 52, 98),
                                  skity::Paint::kFill_Style);
  large_paint.SetFontThreshold(64.f);
  DrawText(canvas, "PATH", x, y, large_paint);
  DrawLabel(canvas, "large text with low font threshold", x, y + 68.f,
            column_width, label_typeface);
  y += 172.f;

  auto emoji_typeface = MatchOrDefault(font_manager, "Segoe UI Emoji",
                                       skity::FontStyle::Normal());
  auto emoji_paint = PaintForText(emoji_typeface, 72.f, Color(24, 78, 74),
                                  skity::Paint::kFill_Style);
  auto emoji_text = Utf8({0x1F600, 0x20, 0x1F680, 0x20, 0x1F4A9});
  canvas->Save();
  canvas->Translate(x, y);
  canvas->Scale(1.35f, 0.85f);
  DrawText(canvas, emoji_text, 0.f, 0.f, emoji_paint);
  canvas->Restore();
  DrawLabel(canvas, "color emoji under non-uniform transform", x, y + 78.f,
            column_width, label_typeface);
}

void DrawColorRegressionCases(
    skity::Canvas* canvas,
    const std::shared_ptr<skity::FontManager>& font_manager,
    const std::shared_ptr<skity::Typeface>& label_typeface, float x, float y,
    float column_width) {
  DrawSectionTitle(canvas, "Color glyph regressions", x, y, label_typeface);
  y += 28.f;
  DrawDivider(canvas, x, y, column_width);
  y += 34.f;

  auto segoe =
      MatchOrDefault(font_manager, "Segoe UI", skity::FontStyle::Normal());
  auto emoji_typeface = MatchOrDefault(font_manager, "Segoe UI Emoji",
                                       skity::FontStyle::Normal());
  auto colrv1_candidate =
      Utf8({0x1FAE0, 0x20, 0x1FAE1, 0x20, 0x1FAF6, 0x20, 0x1F600});
  auto name_paint = PaintForText(label_typeface, 30.f, Color(20, 79, 73));
  DrawText(canvas, "Segoe UI Emoji COLRv1 candidate", x, y, name_paint);
  auto emoji_paint = PaintForText(emoji_typeface, 54.f, Color(20, 79, 73));
  DrawText(canvas, colrv1_candidate, x, y + 58.f, emoji_paint);
  DrawLabel(canvas, "COLRv1 candidate via system emoji font", x, y + 100.f,
            column_width, label_typeface);
  y += 152.f;

  skity::Font normal_emoji_font(emoji_typeface, 56.f);
  skity::Font fake_bold_emoji_font(emoji_typeface, 56.f);
  fake_bold_emoji_font.SetEmbolden(true);
  auto fake_bold_emoji_text = Utf8({0x1F600, 0x20, 0x1F642, 0x20, 0x1F680});
  auto normal_emoji_paint =
      PaintForText(emoji_typeface, 56.f, Color(20, 79, 73));
  DrawGlyphText(canvas, fake_bold_emoji_text, x, y, normal_emoji_font,
                normal_emoji_paint);
  DrawLabel(canvas, "color glyphs without Font::SetEmbolden", x, y + 44.f,
            column_width, label_typeface);
  y += 100.f;

  auto fake_bold_emoji_paint =
      PaintForText(emoji_typeface, 56.f, Color(88, 52, 112));
  DrawGlyphText(canvas, fake_bold_emoji_text, x, y, fake_bold_emoji_font,
                fake_bold_emoji_paint);
  DrawLabel(canvas, "color glyphs with Font::SetEmbolden(true)", x, y + 44.f,
            column_width, label_typeface);
  y += 110.f;

  const std::string cache_sample = "CacheColor AaAa";
  for (const auto& item : std::vector<std::pair<skity::Color, const char*>>{
           {Color(209, 53, 68), "red"},
           {Color(34, 123, 77), "green"},
           {Color(35, 92, 179), "blue"},
           {Color(209, 53, 68), "red again"},
       }) {
    auto paint = PaintForText(segoe, 42.f, item.first);
    DrawText(canvas, cache_sample, x, y, paint);
    DrawLabel(canvas, std::string("same glyph atlas key, ") + item.second, x,
              y + 38.f, column_width, label_typeface);
    y += 78.f;
  }
}

void DrawOverview(skity::Canvas* canvas,
                  const std::shared_ptr<skity::FontManager>& font_manager,
                  const std::shared_ptr<skity::Typeface>& label_typeface) {
  auto title_typeface =
      MatchOrDefault(font_manager, "Segoe UI", skity::FontStyle::Bold());
  auto title_paint = PaintForText(title_typeface, 40.f, Color(18, 27, 38));
  DrawText(canvas, "Windows text example", 36.f, 42.f, title_paint);

  const char* mode = skity::Settings::GetSettings().EnableDWriteFontManager()
                         ? "DWrite font manager"
                         : "legacy Windows font manager";
  std::ostringstream info;
  info << mode << " | families=" << font_manager->CountFamilies()
       << " | default="
       << TypefaceSummary(skity::Typeface::GetDefaultTypeface());
  DrawLabel(canvas, info.str(), 36.f, 66.f, kWindowWidth - 72.f,
            label_typeface);
}

class DWriteTextExample : public skity::example::WindowClient {
 public:
  void OnDraw(skity::GPUContext*, skity::Canvas* canvas) override {
    canvas->Clear(Color(248, 250, 252));

    auto font_manager = skity::FontManager::RefDefault();
    auto label_typeface =
        MatchOrDefault(font_manager, "Segoe UI", skity::FontStyle::Normal());

    DrawOverview(canvas, font_manager, label_typeface);

    constexpr float column_width = 630.f;
    DrawFamilyCases(canvas, font_manager, label_typeface, 40.f, 110.f,
                    column_width);
    DrawWeightSweep(canvas, font_manager, label_typeface, 720.f, 110.f,
                    column_width);
    DrawVariationCases(canvas, font_manager, label_typeface, 40.f, 1160.f,
                       column_width);
    DrawCollectionAndDataCases(canvas, font_manager, label_typeface, 720.f,
                               1160.f, column_width);
    DrawFallbackAndEmoji(canvas, font_manager, label_typeface, 1400.f, 110.f,
                         column_width);
    DrawPaintModes(canvas, font_manager, label_typeface, 1400.f, 1080.f,
                   column_width);
    DrawTransforms(canvas, font_manager, label_typeface, 2080.f, 110.f,
                   column_width);
    DrawColorRegressionCases(canvas, font_manager, label_typeface, 2080.f,
                             980.f, column_width);
  }
};

}  // namespace

int main(int argc, const char** argv) {
  skity::Settings::GetSettings().SetEnableDWriteFontManager(
      kEnableDWriteFontManager);
  DWriteTextExample example;
  std::string title = skity::Settings::GetSettings().EnableDWriteFontManager()
                          ? "Windows Text Example - DWrite"
                          : "Windows Text Example - Legacy";
  return skity::example::StartExampleApp(
      argc, argv, example, static_cast<int>(kWindowWidth),
      static_cast<int>(kWindowHeight), title);
}
