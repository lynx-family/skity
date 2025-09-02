// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/codec/codec.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>

TEST(WebPCodecTest, Create) {
  auto data = skity::Data::MakeFromFileName(SKITY_TEST_WEBP_FILE);

  auto codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec != nullptr);

  auto direct_create_codec = skity::Codec::MakeWebpCodec();

  EXPECT_TRUE(direct_create_codec != nullptr);

  EXPECT_TRUE(direct_create_codec->RecognizeFileType(
      reinterpret_cast<const char*>(data->Bytes()), data->Size()));
}

TEST(WebPCodecTest, Decode) {
  auto data = skity::Data::MakeFromFileName(SKITY_TEST_WEBP_FILE);

  auto codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec != nullptr);

  codec->SetData(data);

  auto multi_frame_decoder = codec->DecodeMultiFrame();

  EXPECT_TRUE(multi_frame_decoder != nullptr);

  EXPECT_EQ(multi_frame_decoder->GetFrameCount(), 7);
  EXPECT_EQ(multi_frame_decoder->GetWidth(), 200);
  EXPECT_EQ(multi_frame_decoder->GetHeight(), 200);

  auto frame_0 = multi_frame_decoder->GetFrameInfo(0);

  EXPECT_TRUE(frame_0 != nullptr);

  auto pixmap = multi_frame_decoder->DecodeFrame(frame_0, nullptr);

  EXPECT_TRUE(pixmap != nullptr);

  EXPECT_EQ(pixmap->Width(), 200);
  EXPECT_EQ(pixmap->Height(), 200);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);

  {
    skity::Bitmap bitmap(pixmap, true);

    auto color = bitmap.GetPixel(0, 0);

    EXPECT_EQ(color, skity::ColorSetARGB(255, 0, 0, 255));
  }

  auto first_frame = codec->Decode();

  EXPECT_TRUE(first_frame != nullptr);
  EXPECT_EQ(first_frame->Width(), 200);
  EXPECT_EQ(first_frame->Height(), 200);
  EXPECT_EQ(first_frame->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(first_frame->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);

  {
    skity::Bitmap bitmap(first_frame, true);

    auto color = bitmap.GetPixel(0, 0);

    EXPECT_EQ(color, skity::ColorSetARGB(255, 0, 0, 255));
  }
}