// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/text/text_example.hpp"

namespace skity::example::text {

void draw_text_with_emoji(skity::Canvas* canvas) {
  skity::Paint text_paint;
  text_paint.SetTypeface(skity::Typeface::GetDefaultTypeface());
  text_paint.SetStrokeWidth(2.0f);
  text_paint.SetAntiAlias(true);
  text_paint.SetColor(Color_RED);
  text_paint.SetStyle(skity::Paint::kFill_Style);

  skity::TextBlobBuilder builder;
  canvas->DrawSimpleText("this is ascii text", 5.f, 100.f, text_paint);

  const char* emoji_font_path = EXAMPLE_IMAGE_ROOT "/NotoColorEmoji.ttf";
  auto emoji_typeface = skity::Typeface::MakeFromFile(emoji_font_path);

  std::vector<std::shared_ptr<Typeface>> typefaces;
  if (emoji_typeface) {
    typefaces.push_back(emoji_typeface);
  }

  auto delegate =
      skity::TypefaceDelegate::CreateSimpleFallbackDelegate(typefaces);

  auto blob = builder.BuildTextBlob("Unicode chars 💩 é É ص", text_paint,
                                    delegate.get());

  canvas->DrawTextBlob(blob.get(), 20.f, 130.f, text_paint);
}

}  // namespace skity::example::text
