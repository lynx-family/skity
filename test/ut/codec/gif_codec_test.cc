// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <cstring>
#include <skity/codec/codec.hpp>
#include <skity/graphic/color.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>

TEST(GIFCodecTest, Create) {
  auto data = skity::Data::MakeFromFileName(SKITY_TEST_MF_GIF_FILE);

  auto codec = skity::Codec::MakeGIFCodec();

  EXPECT_TRUE(codec != nullptr);

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(data->Bytes()), data->Size()));

  auto auto_gen = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(auto_gen != nullptr);
}

TEST(GIFCodecTest, DecodeMultipleFrame) {
  auto data = skity::Data::MakeFromFileName(SKITY_TEST_MF_GIF_FILE);

  auto codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec != nullptr);

  codec->SetData(data);

  auto frame_decoder = codec->DecodeMultiFrame();

  EXPECT_TRUE(frame_decoder != nullptr);

  EXPECT_EQ(frame_decoder->GetFrameCount(), 13);

  EXPECT_EQ(frame_decoder->GetWidth(), 100);
  EXPECT_EQ(frame_decoder->GetHeight(), 100);

  auto frame_11 = frame_decoder->GetFrameInfo(10);

  EXPECT_TRUE(frame_11 != nullptr);

  auto pixmap = std::make_shared<skity::Pixmap>(100, 100);

  pixmap = frame_decoder->DecodeFrame(frame_11, pixmap);

  EXPECT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);

  auto pixel = *(reinterpret_cast<uint32_t*>(pixmap->WritableAddr()));

  EXPECT_EQ(pixel, skity::Color_WHITE);
}

TEST(GIFCodecTest, DecodeSingleFrame) {
  auto data = skity::Data::MakeFromFileName(SKITY_TEST_SF_GIF_FILE);

  auto codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec != nullptr);

  codec->SetData(data);

  auto frame_decoder = codec->DecodeMultiFrame();

  EXPECT_TRUE(frame_decoder != nullptr);

  EXPECT_EQ(frame_decoder->GetFrameCount(), 1);

  auto frame_0 = frame_decoder->GetFrameInfo(0);

  EXPECT_TRUE(frame_0 != nullptr);

  auto pixmap = frame_decoder->DecodeFrame(frame_0, nullptr);

  EXPECT_TRUE(pixmap != nullptr);

  auto pixmap_zero = codec->Decode();

  EXPECT_TRUE(pixmap_zero != nullptr);

  EXPECT_EQ(pixmap_zero->Width(), pixmap->Width());
  EXPECT_EQ(pixmap_zero->Height(), pixmap->Height());
  EXPECT_EQ(pixmap_zero->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);
  EXPECT_EQ(pixmap_zero->GetColorType(), skity::ColorType::kRGBA);

  auto pixmap_addr = pixmap->Addr();
  auto pixmap_zero_addr = pixmap_zero->Addr();

  auto size = pixmap->Width() * pixmap->Height() * 4;

  EXPECT_EQ(std::memcmp(pixmap_addr, pixmap_zero_addr, size), 0);
}
