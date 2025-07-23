// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/mask_filter/mask_filter_example.hpp"

#include <skity/codec/codec.hpp>
#include <skity/skity.hpp>

#include "common/app_utils.hpp"

namespace skity::example::mask_filter {

static std::shared_ptr<skity::Pixmap> load_image(const char* path) {
  auto data = skity::Data::MakeFromFileName(path);

  auto codec = skity::Codec::MakeFromData(data);
  if (!codec) {
    return nullptr;
  }

  codec->SetData(data);
  auto image = codec->Decode();
  if (!image || image->RowBytes() == 0) {
    return nullptr;
  }

  return image;
}

void draw_filter(skity::Canvas* canvas) {
  static auto image_ff = load_image(EXAMPLE_IMAGE_ROOT "/firefox_64.png");

  auto filter = skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 10.f);

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(18);
  paint.SetColor(0xff4285F4);
  paint.SetStrokeCap(skity::Paint::kRound_Cap);

  paint.SetMaskFilter(filter);

  skity::Path path;
  path.MoveTo(10, 10);
  path.QuadTo(256, 64, 128, 128);
  path.QuadTo(10, 192, 250, 250);

  {
    skity::Vec4 colors[] = {
        skity::Vec4{0.f, 1.f, 1.f, 1.f},
        skity::Vec4{0.f, 0.f, 1.f, 1.f},
        skity::Vec4{1.f, 0.f, 0.f, 1.f},
    };

    std::vector<skity::Point> pts = {
        skity::Point{10.f, 10.f, 0.f, 1.f},
        skity::Point{250.f, 250.f, 0.f, 1.f},
    };

    auto lgs = skity::Shader::MakeLinear(pts.data(), colors, nullptr, 3);
    paint.SetShader(lgs);
  }

  canvas->DrawPath(path, paint);

  paint.SetShader(nullptr);

  paint.SetStyle(skity::Paint::kFill_Style);

  canvas->Save();
  canvas->Translate(150.f, 0.f);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 20.f));
  canvas->DrawCircle(100.f, 75.f, 32.f, paint);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(300.f, 0.f);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kSolid, 20.f));
  canvas->DrawCircle(100.f, 75.f, 32.f, paint);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(450.f, 0.f);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kOuter, 20.f));
  canvas->DrawCircle(100.f, 75.f, 32.f, paint);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(600, 0.f);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kInner, 20.f));
  canvas->DrawCircle(100.f, 75.f, 32.f, paint);
  canvas->Restore();

  auto image_rect = skity::Rect::MakeXYWH(
      image_ff->Width(), 0, image_ff->Width(), image_ff->Height());

  canvas->Save();
  canvas->Translate(150.f, 150.f);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 20.f));
  canvas->DrawImage(skity::Image::MakeImage(image_ff), image_rect, &paint);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(300.f, 150.f);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kSolid, 20.f));
  canvas->DrawImage(skity::Image::MakeImage(image_ff), image_rect, &paint);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(450.f, 150.f);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kOuter, 20.f));
  canvas->DrawImage(skity::Image::MakeImage(image_ff), image_rect, &paint);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(600, 150.f);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kInner, 20.f));
  canvas->DrawImage(skity::Image::MakeImage(image_ff), image_rect, &paint);
  canvas->Restore();

  skity::Path path2;
  path2.MoveTo(199, 34);
  path2.LineTo(253, 143);
  path2.LineTo(374, 160);
  path2.LineTo(287, 244);
  path2.LineTo(307, 365);
  path2.LineTo(199, 309);
  path2.LineTo(97, 365);
  path2.LineTo(112, 245);
  path2.LineTo(26, 161);
  path2.LineTo(146, 143);
  path2.Close();

  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 10.f));

  canvas->Save();
  canvas->Translate(300, 200);
  canvas->DrawPath(path2, paint);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(0, 300);
  paint.SetTextSize(40.f);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 5.f));
  paint.SetColor(skity::Color_RED);

  paint.SetStyle(skity::Paint::kFill_Style);

  canvas->DrawSimpleText2("Hello World!", 12, 32, paint);
  paint.SetMaskFilter(nullptr);
  paint.SetColor(0xff4285F4);
  canvas->DrawSimpleText2("Hello World!", 10, 30, paint);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(50, 400);
  static auto image =
      make_rect_image(128, skity::Color_RED, 64, skity::Color_BLUE);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 5.f));
  canvas->DrawImage(skity::Image::MakeImage(image),
                    skity::Rect::MakeWH(image->Width(), image->Height()),
                    &paint);
  canvas->Restore();
}
}  // namespace skity::example::mask_filter
