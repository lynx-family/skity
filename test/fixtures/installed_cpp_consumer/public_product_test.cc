// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// This translation unit consumes installed public headers and libraries only.
#include <gtest/gtest.h>

#include <cstring>
#include <skity/codec/codec.hpp>
#include <skity/skity.hpp>

TEST(InstalledCppProduct, PreservesCanvasAndEncodedPixels) {
  skity::Bitmap bitmap(16, 16);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_NE(canvas, nullptr);
  canvas->Clear(skity::Color_WHITE);
  const auto depth = canvas->GetSaveCount();
  const auto matrix = canvas->GetTotalMatrix();
  EXPECT_EQ(canvas->Save(), depth);
  canvas->Translate(2, 3);
  skity::Paint paint;
  paint.SetColor(skity::Color_BLUE);
  paint.SetAntiAlias(false);
  canvas->DrawRect(skity::Rect::MakeXYWH(1, 1, 5, 6), paint);
  canvas->Restore();
  canvas->Flush();
  EXPECT_EQ(canvas->GetSaveCount(), depth);
  EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
  EXPECT_EQ(bitmap.GetPixel(3, 4), skity::Color_BLUE);
  EXPECT_EQ(bitmap.GetPixel(0, 0), skity::Color_WHITE);
  auto encoder = skity::Codec::MakePngCodec();
  ASSERT_NE(encoder, nullptr);
  auto bytes = encoder->Encode(bitmap.GetPixmap().get());
  ASSERT_NE(bytes, nullptr);
  auto decoder = skity::Codec::MakeFromData(bytes);
  ASSERT_NE(decoder, nullptr);
  decoder->SetData(bytes);
  auto decoded = decoder->Decode();
  ASSERT_NE(decoded, nullptr);
  ASSERT_EQ(decoded->Width(), 16u);
  ASSERT_EQ(decoded->Height(), 16u);
  EXPECT_EQ(decoded->RowBytes(), bitmap.RowBytes());
  EXPECT_EQ(std::memcmp(decoded->Addr(), bitmap.GetPixelAddr(),
                        bitmap.RowBytes() * bitmap.Height()),
            0);
}
