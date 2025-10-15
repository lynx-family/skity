// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/text/text_example.hpp"

namespace skity::example::text {

void draw_text_with_emoji(skity::Canvas* canvas) {
  skity::Paint text_paint;

  static auto font_manager = FontManager::RefDefault();
  // static auto font_set = font_manager->CreateStyleSet(0);
  // static auto typeface = font_set->CreateTypeface(0);
  static auto typeface = font_manager->MatchFamilyStyle(
      "Consolas", FontStyle{FontStyle::Weight::kNormal_Weight,
                            FontStyle::Width::kNormal_Width,
                            FontStyle::Slant::kItalic_Slant});
  text_paint.SetTypeface(typeface);

  text_paint.SetStrokeWidth(2.0f);
  text_paint.SetAntiAlias(true);
  text_paint.SetColor(Color_RED);
  text_paint.SetStrokeColor(Color_BLUE);
  text_paint.SetTextSize(48.f);
  text_paint.SetStyle(skity::Paint::kStroke_Style);

  skity::TextBlobBuilder builder;
  canvas->DrawSimpleText("this is ascii text", 5.f, 100.f, text_paint);
  text_paint.SetStyle(skity::Paint::kFill_Style);
  static auto typeface_cjk = font_manager->MatchFamilyStyleCharacter(
      nullptr,
      FontStyle{FontStyle::Weight::kBlack_Weight,
                FontStyle::Width::kNormal_Width,
                FontStyle::Slant::kUpright_Slant},
      nullptr, 0, 0x7ECF);
  text_paint.SetTypeface(typeface_cjk);
  canvas->DrawSimpleText("你好", 5.f, 300.f, text_paint);

  const char* emoji_font_path = EXAMPLE_IMAGE_ROOT "/NotoColorEmoji.ttf";
  static auto emoji_typeface = skity::Typeface::MakeFromFile(emoji_font_path);

  std::vector<std::shared_ptr<Typeface>> typefaces;
  typefaces.push_back(typeface);
  if (emoji_typeface) {
    typefaces.push_back(emoji_typeface);
  }

  auto delegate =
      skity::TypefaceDelegate::CreateSimpleFallbackDelegate(typefaces);

  auto blob = builder.BuildTextBlob("Unicode chars 💩 é É ص", text_paint,
                                    delegate.get());

  canvas->DrawTextBlob(blob.get(), 20.f, 430.f, text_paint);
}

}  // namespace skity::example::text
