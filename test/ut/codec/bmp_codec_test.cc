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

TEST(BMPCodecTest, RecognizeFileType) {
  auto png_data = skity::Data::MakeFromFileName(SKITY_TEST_PNG_FILE);

  const unsigned char bmp_header[] = {
      'B',  'M',               // signature
      0x36, 0x00, 0x00, 0x00,  // file size = 54
      0x00, 0x00,              // reserved
      0x00, 0x00,              // reserved
      0x36, 0x00, 0x00, 0x00,  // data offset = 54
      0x28, 0x00, 0x00, 0x00,  // info header size = 40
      0x01, 0x00, 0x00, 0x00,  // width = 1
      0x01, 0x00, 0x00, 0x00,  // height = 1
      0x01, 0x00,              // planes = 1
      0x18, 0x00,              // bpp = 24
      0x00, 0x00, 0x00, 0x00,  // compression = BI_RGB
      0x04, 0x00, 0x00, 0x00,  // raw data size = 4
      0x13, 0x0B, 0x00, 0x00,  // x pixels per meter
      0x13, 0x0B, 0x00, 0x00,  // y pixels per meter
      0x00, 0x00, 0x00, 0x00,  // colors used
      0x00, 0x00, 0x00, 0x00,  // important colors
      0x00, 0x00, 0xFF, 0x00,  // pixel data
  };
  auto bmp_data = skity::Data::MakeWithCopy(bmp_header, sizeof(bmp_header));

  auto codec = skity::Codec::MakeFromData(bmp_data);

  ASSERT_TRUE(codec != nullptr) << "MakeFromData returned nullptr for BMP data";

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(bmp_header), sizeof(bmp_header)));
  EXPECT_FALSE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(png_data->Bytes()), png_data->Size()));
}

TEST(BMPCodecTest, Decode24Bit) {
  const unsigned char bmp_24bit[] = {
      'B',  'M',               // signature
      0x36, 0x00, 0x00, 0x00,  // file size = 54 bytes
      0x00, 0x00,              // reserved
      0x00, 0x00,              // reserved
      0x36, 0x00, 0x00, 0x00,  // data offset = 54
      0x28, 0x00, 0x00, 0x00,  // info header size = 40
      0x01, 0x00, 0x00, 0x00,  // width = 1
      0x01, 0x00, 0x00, 0x00,  // height = 1
      0x01, 0x00,              // planes = 1
      0x18, 0x00,              // bpp = 24
      0x00, 0x00, 0x00, 0x00,  // compression = BI_RGB
      0x04, 0x00, 0x00, 0x00,  // raw data size = 4 (padded to 4 bytes)
      0x13, 0x0B, 0x00, 0x00,  // x pixels per meter
      0x13, 0x0B, 0x00, 0x00,  // y pixels per meter
      0x00, 0x00, 0x00, 0x00,  // colors used
      0x00, 0x00, 0x00, 0x00,  // important colors
      0x00, 0x00, 0xFF, 0x00,  // BGR pixel + padding (red)
  };

  auto data = skity::Data::MakeWithCopy(bmp_24bit, sizeof(bmp_24bit));
  auto codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec != nullptr);

  codec->SetData(data);
  auto pixmap = codec->Decode();

  EXPECT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 1);
  EXPECT_EQ(pixmap->Height(), 1);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);

  const auto* addr = reinterpret_cast<const uint8_t*>(pixmap->Addr());
  EXPECT_EQ(addr[0], 0xFF);
  EXPECT_EQ(addr[1], 0x00);
  EXPECT_EQ(addr[2], 0x00);
  EXPECT_EQ(addr[3], 0xFF);
}

TEST(BMPCodecTest, Decode32Bit) {
  const unsigned char bmp_32bit[] = {
      'B',  'M',               // signature
      0x46, 0x00, 0x00, 0x00,  // file size = 70 bytes
      0x00, 0x00,              // reserved
      0x00, 0x00,              // reserved
      0x36, 0x00, 0x00, 0x00,  // data offset = 54
      0x28, 0x00, 0x00, 0x00,  // info header size = 40
      0x01, 0x00, 0x00, 0x00,  // width = 1
      0x01, 0x00, 0x00, 0x00,  // height = 1
      0x01, 0x00,              // planes = 1
      0x20, 0x00,              // bpp = 32
      0x00, 0x00, 0x00, 0x00,  // compression = BI_RGB
      0x04, 0x00, 0x00, 0x00,  // raw data size = 4
      0x13, 0x0B, 0x00, 0x00,  // x pixels per meter
      0x13, 0x0B, 0x00, 0x00,  // y pixels per meter
      0x00, 0x00, 0x00, 0x00,  // colors used
      0x00, 0x00, 0x00, 0x00,  // important colors
      0x00, 0x00, 0xFF, 0x80,  // BGRA pixel (red with 50% alpha)
  };

  auto data = skity::Data::MakeWithCopy(bmp_32bit, sizeof(bmp_32bit));
  auto codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec != nullptr);

  codec->SetData(data);
  auto pixmap = codec->Decode();

  EXPECT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 1);
  EXPECT_EQ(pixmap->Height(), 1);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);

  const auto* addr = reinterpret_cast<const uint8_t*>(pixmap->Addr());
  EXPECT_EQ(addr[0], 0xFF);
  EXPECT_EQ(addr[1], 0x00);
  EXPECT_EQ(addr[2], 0x00);
  EXPECT_EQ(addr[3], 0x80);
}

TEST(BMPCodecTest, Decode8BitWithPalette) {
  std::vector<uint8_t> bmp_8bit(262 + 8, 0);

  bmp_8bit[0] = 'B';
  bmp_8bit[1] = 'M';
  uint32_t file_size = static_cast<uint32_t>(bmp_8bit.size());
  memcpy(bmp_8bit.data() + 2, &file_size, 4);
  uint32_t data_offset = 262;
  memcpy(bmp_8bit.data() + 10, &data_offset, 4);

  uint32_t header_size = 40;
  memcpy(bmp_8bit.data() + 14, &header_size, 4);
  uint32_t width = 2;
  memcpy(bmp_8bit.data() + 18, &width, 4);
  uint32_t height = 2;
  memcpy(bmp_8bit.data() + 22, &height, 4);
  bmp_8bit[26] = 1;
  bmp_8bit[28] = 8;
  uint32_t compression = 0;
  memcpy(bmp_8bit.data() + 30, &compression, 4);
  uint32_t raw_size = 8;
  memcpy(bmp_8bit.data() + 34, &raw_size, 4);
  uint32_t colors_used = 2;
  memcpy(bmp_8bit.data() + 46, &colors_used, 4);

  size_t palette_offset = 54;
  bmp_8bit[palette_offset + 0] = 0x00;
  bmp_8bit[palette_offset + 1] = 0x00;
  bmp_8bit[palette_offset + 2] = 0xFF;
  bmp_8bit[palette_offset + 3] = 0x00;

  bmp_8bit[palette_offset + 4] = 0xFF;
  bmp_8bit[palette_offset + 5] = 0xFF;
  bmp_8bit[palette_offset + 6] = 0xFF;
  bmp_8bit[palette_offset + 7] = 0x00;

  size_t pixel_offset = 262;
  bmp_8bit[pixel_offset + 0] = 0x00;
  bmp_8bit[pixel_offset + 1] = 0x01;

  bmp_8bit[pixel_offset + 4] = 0x00;
  bmp_8bit[pixel_offset + 5] = 0x01;

  auto data = skity::Data::MakeWithCopy(bmp_8bit.data(), bmp_8bit.size());
  auto codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec != nullptr);

  codec->SetData(data);
  auto pixmap = codec->Decode();

  EXPECT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 2);
  EXPECT_EQ(pixmap->Height(), 2);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);
}

TEST(BMPCodecTest, DecodeTopDown) {
  const unsigned char bmp_topdown[] = {
      'B',  'M',               // signature
      0x36, 0x00, 0x00, 0x00,  // file size = 54 bytes
      0x00, 0x00,              // reserved
      0x00, 0x00,              // reserved
      0x36, 0x00, 0x00, 0x00,  // data offset = 54
      0x28, 0x00, 0x00, 0x00,  // info header size = 40
      0x01, 0x00, 0x00, 0x00,  // width = 1
      0xFF, 0xFF, 0xFF, 0xFF,  // height = -1 (top-down)
      0x01, 0x00,              // planes = 1
      0x18, 0x00,              // bpp = 24
      0x00, 0x00, 0x00, 0x00,  // compression = BI_RGB
      0x04, 0x00, 0x00, 0x00,  // raw data size = 4
      0x13, 0x0B, 0x00, 0x00,  // x pixels per meter
      0x13, 0x0B, 0x00, 0x00,  // y pixels per meter
      0x00, 0x00, 0x00, 0x00,  // colors used
      0x00, 0x00, 0x00, 0x00,  // important colors
      0x00, 0xFF, 0x00, 0x00,  // BGR pixel + padding (green)
  };

  auto data = skity::Data::MakeWithCopy(bmp_topdown, sizeof(bmp_topdown));
  auto codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec != nullptr);

  codec->SetData(data);
  auto pixmap = codec->Decode();

  EXPECT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 1);
  EXPECT_EQ(pixmap->Height(), 1);

  const auto* addr = reinterpret_cast<const uint8_t*>(pixmap->Addr());
  EXPECT_EQ(addr[0], 0x00);
  EXPECT_EQ(addr[1], 0xFF);
  EXPECT_EQ(addr[2], 0x00);
  EXPECT_EQ(addr[3], 0xFF);
}

TEST(BMPCodecTest, Encode24Bit) {
  skity::Pixmap pixmap(1, 1, skity::AlphaType::kOpaque_AlphaType,
                       skity::ColorType::kRGBA);

  {
    auto addr = reinterpret_cast<uint8_t*>(pixmap.WritableAddr());
    addr[0] = 0xFF;
    addr[1] = 0x00;
    addr[2] = 0x00;
    addr[3] = 0xFF;
  }

  const unsigned char bmp_header[] = {
      'B',  'M',  0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36,
      0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  };
  auto codec_data = skity::Data::MakeWithCopy(bmp_header, sizeof(bmp_header));
  auto codec = skity::Codec::MakeFromData(codec_data);

  ASSERT_TRUE(codec != nullptr);

  auto data = codec->Encode(&pixmap);

  EXPECT_TRUE(data != nullptr);

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(data->Bytes()), data->Size()));

  codec->SetData(data);

  auto decode_pixmap = codec->Decode();

  EXPECT_TRUE(decode_pixmap != nullptr);
  EXPECT_EQ(decode_pixmap->Width(), 1);
  EXPECT_EQ(decode_pixmap->Height(), 1);
  EXPECT_EQ(decode_pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(decode_pixmap->GetAlphaType(),
            skity::AlphaType::kUnpremul_AlphaType);

  const auto* decode_addr =
      reinterpret_cast<const uint8_t*>(decode_pixmap->Addr());
  EXPECT_EQ(decode_addr[0], 0xFF);
  EXPECT_EQ(decode_addr[1], 0x00);
  EXPECT_EQ(decode_addr[2], 0x00);
  EXPECT_EQ(decode_addr[3], 0xFF);
}

TEST(BMPCodecTest, Encode32BitWithAlpha) {
  skity::Pixmap pixmap(1, 1, skity::AlphaType::kUnpremul_AlphaType,
                       skity::ColorType::kRGBA);

  {
    auto addr = reinterpret_cast<uint8_t*>(pixmap.WritableAddr());
    addr[0] = 0xFF;
    addr[1] = 0x00;
    addr[2] = 0x00;
    addr[3] = 0x80;
  }

  const unsigned char bmp_header[] = {
      'B',  'M',  0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36,
      0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  };
  auto codec_data = skity::Data::MakeWithCopy(bmp_header, sizeof(bmp_header));
  auto codec = skity::Codec::MakeFromData(codec_data);

  ASSERT_TRUE(codec != nullptr);

  auto data = codec->Encode(&pixmap);

  EXPECT_TRUE(data != nullptr);

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(data->Bytes()), data->Size()));

  codec->SetData(data);

  auto decode_pixmap = codec->Decode();

  EXPECT_TRUE(decode_pixmap != nullptr);
  EXPECT_EQ(decode_pixmap->Width(), 1);
  EXPECT_EQ(decode_pixmap->Height(), 1);
  EXPECT_EQ(decode_pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(decode_pixmap->GetAlphaType(),
            skity::AlphaType::kUnpremul_AlphaType);

  const auto* decode_addr =
      reinterpret_cast<const uint8_t*>(decode_pixmap->Addr());
  EXPECT_EQ(decode_addr[0], 0xFF);
  EXPECT_EQ(decode_addr[1], 0x00);
  EXPECT_EQ(decode_addr[2], 0x00);
  EXPECT_EQ(decode_addr[3], 0x80);
}

TEST(BMPCodecTest, EncodeBGRA) {
  skity::Pixmap pixmap(1, 1, skity::AlphaType::kUnpremul_AlphaType,
                       skity::ColorType::kBGRA);

  {
    auto addr = reinterpret_cast<uint8_t*>(pixmap.WritableAddr());
    addr[0] = 0x00;
    addr[1] = 0x00;
    addr[2] = 0xFF;
    addr[3] = 0x80;
  }

  const unsigned char bmp_header[] = {
      'B',  'M',  0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36,
      0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  };
  auto codec_data = skity::Data::MakeWithCopy(bmp_header, sizeof(bmp_header));
  auto codec = skity::Codec::MakeFromData(codec_data);

  ASSERT_TRUE(codec != nullptr);

  auto data = codec->Encode(&pixmap);

  EXPECT_TRUE(data != nullptr);

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(data->Bytes()), data->Size()));

  codec->SetData(data);

  auto decode_pixmap = codec->Decode();

  EXPECT_TRUE(decode_pixmap != nullptr);
  EXPECT_EQ(decode_pixmap->Width(), 1);
  EXPECT_EQ(decode_pixmap->Height(), 1);
  EXPECT_EQ(decode_pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(decode_pixmap->GetAlphaType(),
            skity::AlphaType::kUnpremul_AlphaType);

  const auto* decode_addr =
      reinterpret_cast<const uint8_t*>(decode_pixmap->Addr());
  EXPECT_EQ(decode_addr[0], 0xFF);
  EXPECT_EQ(decode_addr[1], 0x00);
  EXPECT_EQ(decode_addr[2], 0x00);
  EXPECT_EQ(decode_addr[3], 0x80);
}

TEST(BMPCodecTest, EncodeWithCanvas) {
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

  const unsigned char bmp_header[] = {
      'B',  'M',  0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36,
      0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  };
  auto codec_data = skity::Data::MakeWithCopy(bmp_header, sizeof(bmp_header));
  auto codec = skity::Codec::MakeFromData(codec_data);

  ASSERT_TRUE(codec != nullptr);

  auto data = codec->Encode(bitmap.GetPixmap().get());

  EXPECT_TRUE(data != nullptr);

  EXPECT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(data->Bytes()), data->Size()));

  codec->SetData(data);

  auto decode_pixmap = codec->Decode();

  EXPECT_TRUE(decode_pixmap != nullptr);
  EXPECT_EQ(decode_pixmap->Width(), 128);
  EXPECT_EQ(decode_pixmap->Height(), 128);
}

TEST(BMPCodecTest, InvalidHeader) {
  const unsigned char invalid_bmp[] = {'X', 'Y', 0x00, 0x00, 0x00, 0x00};

  auto data = skity::Data::MakeWithCopy(invalid_bmp, sizeof(invalid_bmp));
  auto codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec == nullptr);
}

TEST(BMPCodecTest, InvalidSize) {
  const unsigned char too_short[] = {'B', 'M'};

  auto data = skity::Data::MakeWithCopy(too_short, sizeof(too_short));
  auto codec = skity::Codec::MakeFromData(data);

  EXPECT_TRUE(codec == nullptr);
}