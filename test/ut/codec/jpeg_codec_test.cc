// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/codec/codec.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/render/canvas.hpp>

TEST(JPEGCodecTest, RecognizeFileType) {
  auto png_data = skity::Data::MakeFromFileName(SKITY_TEST_PNG_FILE);
  auto jpeg_data = skity::Data::MakeFromFileName(SKITY_TEST_JPEG_FILE);

  auto codec = skity::Codec::MakeJPEGCodec();

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(jpeg_data->Bytes()), jpeg_data->Size()));
  EXPECT_FALSE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(png_data->Bytes()), png_data->Size()));
}

TEST(JPEGCodecTest, Decode) {
  auto jpeg_data = skity::Data::MakeFromFileName(SKITY_TEST_JPEG_FILE);

  auto codec = skity::Codec::MakeFromData(jpeg_data);

  codec->SetData(jpeg_data);

  auto pixmap = codec->Decode();

  EXPECT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 133);
  EXPECT_EQ(pixmap->Height(), 100);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);

  const unsigned char empty_jpeg[] = {0xFF, 0xD8, 0xFF};

  EXPECT_FALSE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(empty_jpeg), sizeof(empty_jpeg)));
}

TEST(JPEGCodecTest, Encode) {
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

  auto codec = skity::Codec::MakeJPEGCodec();

  auto data = codec->Encode(bitmap.GetPixmap().get());

  EXPECT_TRUE(data != nullptr);

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(data->Bytes()), data->Size()));
}

TEST(JPEGCodecTest, EncodeBGRA) {
  skity::Pixmap pixmap(1, 1, skity::AlphaType::kUnpremul_AlphaType,
                       skity::ColorType::kBGRA);

  {
    auto addr = reinterpret_cast<uint32_t*>(pixmap.WritableAddr());

    *addr = skity::ColorSetARGB(128, 255, 0, 0);
  }

  auto codec = skity::Codec::MakeJPEGCodec();

  auto jpeg_data = codec->Encode(&pixmap);

  EXPECT_TRUE(jpeg_data != nullptr);

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(jpeg_data->Bytes()), jpeg_data->Size()));

  codec->SetData(jpeg_data);

  auto decode_pixmap = codec->Decode();

  EXPECT_TRUE(decode_pixmap != nullptr);
  EXPECT_EQ(decode_pixmap->Width(), 1);
  EXPECT_EQ(decode_pixmap->Height(), 1);
  EXPECT_EQ(decode_pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(decode_pixmap->GetAlphaType(),
            skity::AlphaType::kUnpremul_AlphaType);

  const auto* decode_addr =
      reinterpret_cast<const uint32_t*>(decode_pixmap->Addr());

  EXPECT_EQ(*decode_addr, skity::ColorSetARGB(255, 0, 0, 128));
}
