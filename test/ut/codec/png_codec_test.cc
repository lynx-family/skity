// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/codec/codec.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/render/canvas.hpp>

TEST(PNGCodecTest, Create) {
  // create directly
  auto codec = skity::Codec::MakePngCodec();
  EXPECT_TRUE(codec != nullptr);

  // create from data
  auto data = skity::Data::MakeFromFileName(SKITY_TEST_PNG_FILE);
  codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec != nullptr);
}

TEST(PNGCodecTest, Decode) {
  auto data = skity::Data::MakeFromFileName(SKITY_TEST_PNG_FILE);
  auto codec = skity::Codec::MakeFromData(data);
  EXPECT_TRUE(codec != nullptr);

  codec->SetData(data);

  auto pixmap = codec->Decode();

  EXPECT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 64);
  EXPECT_EQ(pixmap->Height(), 64);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);

  const unsigned char empty_png_header[] = {0x89, 0x50, 0x4e, 0x47,
                                            0x0d, 0x0a, 0x1a, 0x0a};

  data = skity::Data::MakeWithCopy(empty_png_header, sizeof(empty_png_header));

  EXPECT_TRUE(
      codec->RecognizeFileType(reinterpret_cast<const char*>(empty_png_header),
                               sizeof(empty_png_header)));

  codec->SetData(data);
  pixmap = codec->Decode();

  EXPECT_TRUE(pixmap == nullptr);

  // 1x1 RGBA format PNG (black color, fully opaque)
  const unsigned char one_by_one_rgba_png[] = {
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A,
      0x1A, 0x0A,  // PNG signature
      0x00, 0x00, 0x00, 0x0D, 0x49, 0x48,
      0x44, 0x52,  // IHDR
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
      0x00, 0x01,  // width=1, height=1
      0x08, 0x06, 0x00, 0x00, 0x00, 0x1F,
      0x15, 0xC4, 0x89,  // bit depth=8, RGBA
      0x00, 0x00, 0x00, 0x0A, 0x49, 0x44,
      0x41, 0x54,  // IDAT
      0x78, 0x9C, 0x63, 0x00, 0x01, 0x00,
      0x00, 0x05,  // zlib-compressed (5 bytes: filter+RGBA)
      0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4,  // Adler32 checksum
      0x75, 0xCA, 0x82, 0x5F,              // CRC
      0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
      0x4E, 0x44,              // IEND
      0xAE, 0x42, 0x60, 0x82,  // CRC
  };

  data = skity::Data::MakeWithCopy(one_by_one_rgba_png,
                                   sizeof(one_by_one_rgba_png));

  codec = skity::Codec::MakePngCodec();

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(one_by_one_rgba_png),
      sizeof(one_by_one_rgba_png)));

  codec->SetData(data);

  pixmap = codec->Decode();

  EXPECT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 1);
  EXPECT_EQ(pixmap->Height(), 1);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);
}

TEST(PNGCodecTest, Encode) {
  skity::Bitmap bitmap(128, 128, skity::AlphaType::kUnpremul_AlphaType,
                       skity::ColorType::kRGBA);

  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);

  canvas->Clear(skity::Color_TRANSPARENT);

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);
  paint.SetAlphaF(0.5f);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(5.f);

  canvas->DrawCircle(64, 64, 50, paint);

  auto codec = skity::Codec::MakePngCodec();

  auto data = codec->Encode(bitmap.GetPixmap().get());

  EXPECT_TRUE(data != nullptr);

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(data->Bytes()), data->Size()));
}
