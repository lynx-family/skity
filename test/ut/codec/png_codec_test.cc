// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#endif

#include <cmath>
#include <cstring>
#include <skity/codec/codec.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/render/canvas.hpp>
#include <utility>
#include <vector>

#include "module/codec/src/codec/codec_priv.hpp"
#include "test/ut/graphic/color_space_test_util.hpp"

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
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,  // PNG signature
      0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,  // IHDR
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,  // width=1, height=1
      0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4,
      0x89,                                            // bit depth=8, RGBA
      0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54,  // IDAT
      0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05,
      0x00, 0x01,              // zlib stream (filter + RGBA)
      0x0D, 0x0A, 0x2D, 0xB4,  // CRC
      0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44,  // IEND
      0xAE, 0x42, 0x60, 0x82,                          // CRC
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

TEST(PNGCodecTest, EncodePremulAlpha) {
  skity::Pixmap pixmap(1, 1, skity::AlphaType::kPremul_AlphaType);

  {
    auto addr = reinterpret_cast<uint32_t*>(pixmap.WritableAddr());

    *addr = skity::ColorSetARGB(128, 128, 0, 0);
  }

  auto codec = skity::Codec::MakePngCodec();

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

  auto decode_color = reinterpret_cast<const uint32_t*>(decode_pixmap->Addr());
  EXPECT_EQ(*decode_color, skity::ColorSetARGB(128, 255, 0, 0));
}

TEST(PNGCodecTest, EncodeBGRA) {
  skity::Pixmap pixmap(1, 1, skity::AlphaType::kUnpremul_AlphaType,
                       skity::ColorType::kBGRA);

  {
    auto addr = reinterpret_cast<uint32_t*>(pixmap.WritableAddr());

    *addr = skity::ColorSetARGB(128, 255, 0, 0);
  }

  auto codec = skity::Codec::MakePngCodec();

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

  auto decode_color = reinterpret_cast<const uint32_t*>(decode_pixmap->Addr());
  EXPECT_EQ(*decode_color, skity::ColorSetARGB(128, 0, 0, 255));
}

namespace {

// Primary xy chromaticity extracted from a gamut matrix column / white point
// from the row sums.
uint32_t Crc32(const uint8_t* data, size_t size) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < size; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++) {
      uint32_t mask = static_cast<uint32_t>(-static_cast<int32_t>(crc & 1));
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

uint32_t ReadBE32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
         (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

// Splices a chunk with the given type and payload right before the first
// IDAT.
std::shared_ptr<skity::Data> InsertChunkBeforeIDAT(const skity::Data* png,
                                                   const char type[5],
                                                   const uint8_t* payload,
                                                   size_t payload_len) {
  const uint8_t* bytes = static_cast<const uint8_t*>(png->RawData());
  size_t size = png->Size();
  size_t pos = 8;
  while (pos + 12 <= size) {
    uint32_t length = ReadBE32(bytes + pos);
    if (std::memcmp(bytes + pos + 4, "IDAT", 4) == 0) {
      std::vector<uint8_t> out(bytes, bytes + pos);
      std::vector<uint8_t> chunk(4 + payload_len);
      std::memcpy(chunk.data(), type, 4);
      if (payload_len > 0) {
        std::memcpy(chunk.data() + 4, payload, payload_len);
      }
      uint32_t crc = Crc32(chunk.data(), chunk.size());
      uint8_t header[4] = {static_cast<uint8_t>(payload_len >> 24),
                           static_cast<uint8_t>(payload_len >> 16),
                           static_cast<uint8_t>(payload_len >> 8),
                           static_cast<uint8_t>(payload_len)};
      out.insert(out.end(), header, header + 4);
      out.insert(out.end(), chunk.begin(), chunk.end());
      out.push_back(static_cast<uint8_t>(crc >> 24));
      out.push_back(static_cast<uint8_t>(crc >> 16));
      out.push_back(static_cast<uint8_t>(crc >> 8));
      out.push_back(static_cast<uint8_t>(crc));
      out.insert(out.end(), bytes + pos, bytes + size);
      return skity::Data::MakeWithCopy(out.data(), out.size());
    }
    pos += 12 + length;
  }
  return nullptr;
}

// Flips a bit in the CRC of the first chunk of the given type, leaving the
// payload intact.
std::shared_ptr<skity::Data> CorruptChunkCRC(const skity::Data* png,
                                             const char type[5]) {
  const uint8_t* bytes = static_cast<const uint8_t*>(png->RawData());
  size_t size = png->Size();
  size_t pos = 8;
  while (pos + 12 <= size) {
    uint32_t length = ReadBE32(bytes + pos);
    if (std::memcmp(bytes + pos + 4, type, 4) == 0) {
      std::vector<uint8_t> out(bytes, bytes + size);
      out[pos + 8 + length + 3] ^= 0xFF;
      return skity::Data::MakeWithCopy(out.data(), out.size());
    }
    pos += 12 + length;
  }
  return nullptr;
}

// Returns the gAMA payload (fixed point) or 0 when the file has none.
uint32_t FindGAMAValue(const skity::Data* png) {
  const uint8_t* bytes = static_cast<const uint8_t*>(png->RawData());
  size_t size = png->Size();
  size_t pos = 8;
  while (pos + 12 <= size) {
    uint32_t length = ReadBE32(bytes + pos);
    if (std::memcmp(bytes + pos + 4, "gAMA", 4) == 0 && length >= 4) {
      return ReadBE32(bytes + pos + 8);
    }
    pos += 12 + length;
  }
  return 0;
}

// Removes a chunk of the given type (first occurrence).
std::shared_ptr<skity::Data> RemoveChunkFromPNG(const skity::Data* png,
                                                const char type[5]) {
  const uint8_t* bytes = static_cast<const uint8_t*>(png->RawData());
  size_t size = png->Size();
  size_t pos = 8;
  while (pos + 12 <= size) {
    uint32_t length = ReadBE32(bytes + pos);
    if (std::memcmp(bytes + pos + 4, type, 4) == 0) {
      std::vector<uint8_t> out(bytes, bytes + pos);
      out.insert(out.end(), bytes + pos + 12 + length, bytes + size);
      return skity::Data::MakeWithCopy(out.data(), out.size());
    }
    pos += 12 + length;
  }
  return nullptr;
}

// Replaces the cHRM payload with the given fixed point values.
std::shared_ptr<skity::Data> MutateCHRMInPNG(const skity::Data* png,
                                             const uint32_t values[8]) {
  const uint8_t* bytes = static_cast<const uint8_t*>(png->RawData());
  size_t size = png->Size();
  size_t pos = 8;
  while (pos + 12 <= size) {
    uint32_t length = ReadBE32(bytes + pos);
    if (std::memcmp(bytes + pos + 4, "cHRM", 4) == 0) {
      std::vector<uint8_t> out(bytes, bytes + pos + 8);
      std::vector<uint8_t> chunk(4 + 32);
      std::memcpy(chunk.data(), "cHRM", 4);
      for (int i = 0; i < 8; i++) {
        chunk[4 + i * 4] = static_cast<uint8_t>(values[i] >> 24);
        chunk[5 + i * 4] = static_cast<uint8_t>(values[i] >> 16);
        chunk[6 + i * 4] = static_cast<uint8_t>(values[i] >> 8);
        chunk[7 + i * 4] = static_cast<uint8_t>(values[i]);
      }
      // The CRC covers type + payload, but the prefix above already wrote
      // the chunk type, so only the payload bytes are appended.
      uint32_t crc = Crc32(chunk.data(), chunk.size());
      out.insert(out.end(), chunk.begin() + 4, chunk.end());
      out.push_back(static_cast<uint8_t>(crc >> 24));
      out.push_back(static_cast<uint8_t>(crc >> 16));
      out.push_back(static_cast<uint8_t>(crc >> 8));
      out.push_back(static_cast<uint8_t>(crc));
      out.insert(out.end(), bytes + pos + 12 + length, bytes + size);
      return skity::Data::MakeWithCopy(out.data(), out.size());
    }
    pos += 12 + length;
  }
  return nullptr;
}

}  // namespace

TEST(PNGCodecTest, EncodeDecodeColorSpaceRoundTrip) {
  // Gamma 2.2 transfer + Display P3 primaries: expressible exactly through
  // the gAMA / cHRM chunks.
  float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  float p3_gamut[9];
  std::memcpy(p3_gamut, skity::ColorSpace::DisplayP3()->gamut_xyz,
              sizeof(p3_gamut));
  auto space = skity::ColorSpace::MakeRGB(tf, p3_gamut);
  ASSERT_NE(space, nullptr);

  skity::Pixmap pixmap(4, 4);
  ASSERT_TRUE(pixmap.SetColorInfo(skity::kUnpremul_AlphaType,
                                  skity::ColorType::kRGBA, space));

  auto codec = skity::Codec::MakePngCodec();
  auto data = codec->Encode(&pixmap);
  ASSERT_TRUE(data != nullptr);

  codec->SetData(data);
  auto decoded = codec->Decode();
  ASSERT_TRUE(decoded != nullptr);

  const auto& out_space = *decoded->GetColorSpace();
  EXPECT_EQ(out_space.kind, skity::ColorSpace::Kind::kParametric);
  // gAMA quantizes to 1/100000: the canonical 1/2.2 is 45455, which reads
  // back as 2.19998 through the float reciprocal.
  EXPECT_NEAR(out_space.transfer_fn[0], 2.2f, 1e-4f);
  EXPECT_EQ(out_space.transfer_fn[1], 1.f);
  // cHRM stores chromaticities * 100000; the primaries survive the
  // extraction -> chunk -> solve roundtrip. The decoded matrix is
  // D50-adapted, so the native chromaticities come from undoing the
  // adaptation with the reference implementation, not from its columns.
  auto native =
      color_space_test_util::NativeChromaticities(out_space.gamut_xyz);
  EXPECT_NEAR(native.wx, 0.3127, 3e-5);
  EXPECT_NEAR(native.wy, 0.3290, 3e-5);
  EXPECT_NEAR(native.rx, 0.680, 3e-5);
  EXPECT_NEAR(native.ry, 0.320, 3e-5);
  EXPECT_NEAR(native.gx, 0.265, 3e-5);
  EXPECT_NEAR(native.gy, 0.690, 3e-5);
  EXPECT_NEAR(native.bx, 0.150, 3e-5);
  EXPECT_NEAR(native.by, 0.060, 3e-5);
  // The adapted matrix itself reproduces the built-in Display P3 table.
  auto p3_reference = skity::ColorSpace::DisplayP3();
  for (int i = 0; i < 9; i++) {
    EXPECT_NEAR(out_space.gamut_xyz[i], p3_reference->gamut_xyz[i], 2e-4f);
  }
}

TEST(PNGCodecTest, EncodeDecodeDefaultsToSRGBWithoutChunks) {
  skity::Pixmap pixmap(4, 4);
  auto codec = skity::Codec::MakePngCodec();
  auto data = codec->Encode(&pixmap);
  ASSERT_TRUE(data != nullptr);

  codec->SetData(data);
  auto decoded = codec->Decode();
  ASSERT_TRUE(decoded != nullptr);
  // No color chunks written: the decode leaves the space undeclared, which
  // is the sRGB working space.
  EXPECT_EQ(decoded->GetColorSpace(), nullptr);
  EXPECT_TRUE(skity::ColorSpaceIsSRGB(decoded->GetColorSpace()));
}

TEST(PNGCodecTest, SRGBChunkOverridesGamma) {
  // Encode with a gamma-tagged pixmap so the file carries gAMA, then splice
  // an sRGB chunk: per the PNG spec sRGB wins and the decode falls back to
  // the default sRGB tag.
  float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  float p3_gamut[9];
  std::memcpy(p3_gamut, skity::ColorSpace::DisplayP3()->gamut_xyz,
              sizeof(p3_gamut));
  auto space = skity::ColorSpace::MakeRGB(tf, p3_gamut);
  ASSERT_NE(space, nullptr);
  skity::Pixmap pixmap(4, 4);
  ASSERT_TRUE(pixmap.SetColorInfo(skity::kUnpremul_AlphaType,
                                  skity::ColorType::kRGBA, space));

  auto codec = skity::Codec::MakePngCodec();
  auto data = codec->Encode(&pixmap);
  ASSERT_TRUE(data != nullptr);

  const uint8_t srgb_intent = 0;
  auto spliced = InsertChunkBeforeIDAT(data.get(), "sRGB", &srgb_intent,
                                       sizeof(srgb_intent));
  ASSERT_TRUE(spliced != nullptr);
  ASSERT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(spliced->RawData()), spliced->Size()));

  codec->SetData(spliced);
  auto decoded = codec->Decode();
  ASSERT_TRUE(decoded != nullptr);
  // sRGB wins per the PNG spec and the parametric description is dropped:
  // the decode falls back to undeclared (sRGB).
  EXPECT_EQ(decoded->GetColorSpace(), nullptr);
  EXPECT_TRUE(skity::ColorSpaceIsSRGB(decoded->GetColorSpace()));
}

TEST(PNGCodecTest, GammaOnlyColorSpaceRoundTrip) {
  // Gamma 2.2 with sRGB primaries: only gAMA is written; the decode must
  // fill the gamut from the sRGB defaults.
  float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  auto space =
      skity::ColorSpace::MakeRGB(tf, skity::ColorSpace::SRGB()->gamut_xyz);
  ASSERT_NE(space, nullptr);

  skity::Pixmap pixmap(4, 4);
  ASSERT_TRUE(pixmap.SetColorInfo(skity::kUnpremul_AlphaType,
                                  skity::ColorType::kRGBA, space));
  auto codec = skity::Codec::MakePngCodec();
  auto data = codec->Encode(&pixmap);
  ASSERT_TRUE(data != nullptr);

  codec->SetData(data);
  auto decoded = codec->Decode();
  ASSERT_TRUE(decoded != nullptr);
  const auto& out_space = *decoded->GetColorSpace();
  EXPECT_EQ(out_space.kind, skity::ColorSpace::Kind::kParametric);
  // gAMA quantizes to 1/100000: the canonical 1/2.2 is 45455, which reads
  // back as 2.19998 through the float reciprocal.
  EXPECT_NEAR(out_space.transfer_fn[0], 2.2f, 1e-4f);
  // The gamut falls back to the sRGB constants (exact memcpy).
  auto srgb = skity::ColorSpace::SRGB();
  EXPECT_EQ(std::memcmp(out_space.gamut_xyz, srgb->gamut_xyz,
                        sizeof(out_space.gamut_xyz)),
            0);
}

TEST(PNGCodecTest, ChromaOnlyDefaultsTransferToSRGBCurve) {
  // Strip gAMA from a gamma+primaries encode: the decode must keep the P3
  // gamut but fall back to the sRGB transfer curve.
  float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  float p3_gamut[9];
  std::memcpy(p3_gamut, skity::ColorSpace::DisplayP3()->gamut_xyz,
              sizeof(p3_gamut));
  auto space = skity::ColorSpace::MakeRGB(tf, p3_gamut);
  ASSERT_NE(space, nullptr);
  skity::Pixmap pixmap(4, 4);
  ASSERT_TRUE(pixmap.SetColorInfo(skity::kUnpremul_AlphaType,
                                  skity::ColorType::kRGBA, space));
  auto codec = skity::Codec::MakePngCodec();
  auto data = codec->Encode(&pixmap);
  ASSERT_TRUE(data != nullptr);

  auto chroma_only = RemoveChunkFromPNG(data.get(), "gAMA");
  ASSERT_TRUE(chroma_only != nullptr);

  codec->SetData(chroma_only);
  auto decoded = codec->Decode();
  ASSERT_TRUE(decoded != nullptr);
  const auto& out_space = *decoded->GetColorSpace();
  EXPECT_EQ(out_space.kind, skity::ColorSpace::Kind::kParametric);
  auto srgb = skity::ColorSpace::SRGB();
  EXPECT_EQ(std::memcmp(out_space.transfer_fn, srgb->transfer_fn,
                        sizeof(out_space.transfer_fn)),
            0);
  auto native =
      color_space_test_util::NativeChromaticities(out_space.gamut_xyz);
  EXPECT_NEAR(native.rx, 0.680, 3e-5);
  EXPECT_NEAR(native.ry, 0.320, 3e-5);
}

TEST(PNGCodecTest, TamperedChromaLayering) {
  // Well-formed replacement primaries decode into the tagged parametric
  // space; unphysical ones are dropped by the parser-level validation with
  // the decode still succeeding (the math behind the rejection is covered
  // directly by GamutFromChromaticitiesMatchesDisplayP3).
  float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  float p3_gamut[9];
  std::memcpy(p3_gamut, skity::ColorSpace::DisplayP3()->gamut_xyz,
              sizeof(p3_gamut));
  auto space = skity::ColorSpace::MakeRGB(tf, p3_gamut);
  ASSERT_NE(space, nullptr);
  skity::Pixmap pixmap(4, 4);
  ASSERT_TRUE(pixmap.SetColorInfo(skity::kUnpremul_AlphaType,
                                  skity::ColorType::kRGBA, space));
  auto codec = skity::Codec::MakePngCodec();
  auto data = codec->Encode(&pixmap);
  ASSERT_TRUE(data != nullptr);

  // Well-formed replacement primaries (sRGB / BT.709): decode succeeds and
  // the resulting gamut carries those primaries.
  uint32_t bt704_chrm[8] = {31270, 32900, 64000, 33000,
                            30000, 60000, 15000, 6000};
  auto retagged = MutateCHRMInPNG(data.get(), bt704_chrm);
  ASSERT_TRUE(retagged != nullptr);
  codec->SetData(retagged);
  auto decoded = codec->Decode();
  ASSERT_TRUE(decoded != nullptr);
  const auto& out_space = *decoded->GetColorSpace();
  EXPECT_EQ(out_space.kind, skity::ColorSpace::Kind::kParametric);
  // gAMA quantizes to 1/100000: the canonical 1/2.2 is 45455, which reads
  // back as 2.19998 through the float reciprocal.
  EXPECT_NEAR(out_space.transfer_fn[0], 2.2f, 1e-4f);
  auto native =
      color_space_test_util::NativeChromaticities(out_space.gamut_xyz);
  EXPECT_NEAR(native.rx, 0.640, 3e-5);
  EXPECT_NEAR(native.ry, 0.330, 3e-5);

  // Unphysical primaries (red x + y > 1): libpng still decodes the pixels,
  // and the parser-level validation drops the bad chroma — gAMA keeps
  // applying while the gamut falls back to the sRGB defaults.
  uint32_t bad_chrm[8] = {31270, 32900, 90000, 95000,
                          26500, 69500, 15000, 6000};
  auto tampered = MutateCHRMInPNG(data.get(), bad_chrm);
  ASSERT_TRUE(tampered != nullptr);
  codec->SetData(tampered);
  auto degraded = codec->Decode();
  ASSERT_TRUE(degraded != nullptr);
  auto degraded_space = degraded->GetColorSpace();
  EXPECT_EQ(degraded_space->kind, skity::ColorSpace::Kind::kParametric);
  EXPECT_NEAR(degraded_space->transfer_fn[0], 2.2f, 1e-4f);
  auto srgb = skity::ColorSpace::SRGB();
  EXPECT_EQ(std::memcmp(degraded_space->gamut_xyz, srgb->gamut_xyz,
                        sizeof(degraded_space->gamut_xyz)),
            0);
}

TEST(PNGCodecTest, GammaChunkCarriesEncodingGamma) {
  // Independent check of the write direction: gAMA stores the encoding
  // gamma, and the PNG spec's canonical value for 1/2.2 is 45455.
  float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  auto space =
      skity::ColorSpace::MakeRGB(tf, skity::ColorSpace::SRGB()->gamut_xyz);
  ASSERT_NE(space, nullptr);
  skity::Pixmap pixmap(4, 4);
  ASSERT_TRUE(pixmap.SetColorInfo(skity::kUnpremul_AlphaType,
                                  skity::ColorType::kRGBA, space));
  auto codec = skity::Codec::MakePngCodec();
  auto data = codec->Encode(&pixmap);
  ASSERT_TRUE(data != nullptr);

  EXPECT_EQ(FindGAMAValue(data.get()), 45455u);
}

namespace {

// Encodes an untagged gray pixmap and splices a gAMA chunk with the given
// fixed-point value before the IDAT, producing a file whose color metadata
// is independent of the encoder under test.
std::shared_ptr<skity::Data> MakeGammaPNG(uint32_t gama, uint8_t gray) {
  skity::Pixmap pixmap(1, 1, skity::kUnpremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto addr = pixmap.WritableAddr8(0, 0);
  addr[0] = addr[1] = addr[2] = gray;
  addr[3] = 255;
  auto codec = skity::Codec::MakePngCodec();
  auto data = codec->Encode(&pixmap);
  if (!data) {
    return nullptr;
  }
  uint8_t payload[4] = {
      static_cast<uint8_t>(gama >> 24), static_cast<uint8_t>(gama >> 16),
      static_cast<uint8_t>(gama >> 8), static_cast<uint8_t>(gama)};
  return InsertChunkBeforeIDAT(data.get(), "gAMA", payload, sizeof(payload));
}

}  // namespace

TEST(PNGCodecTest, GammaPNGDecodesSourceEncodedBytes) {
  // gAMA=45455 (1/2.2): the decoder must hand out the file's own encoded
  // bytes and tag them with the source space; converting once at sampling
  // time lands near the input value, not the brightened double conversion.
  auto file = MakeGammaPNG(45455, 128);
  ASSERT_TRUE(file != nullptr);
  auto codec = skity::Codec::MakePngCodec();
  codec->SetData(file);
  auto decoded = codec->Decode();
  ASSERT_TRUE(decoded != nullptr);

  // Source encoding preserved: gray 128 stays 128.
  auto pixel = static_cast<const uint8_t*>(decoded->Addr());
  EXPECT_EQ(pixel[0], 128);
  EXPECT_EQ(pixel[1], 128);
  EXPECT_EQ(pixel[2], 128);

  auto space = decoded->GetColorSpace();
  ASSERT_TRUE(space != nullptr);
  EXPECT_EQ(space->kind, skity::ColorSpace::Kind::kParametric);
  // 45455 reads back as the decode exponent 1/0.45455 = 2.19998.
  EXPECT_NEAR(space->transfer_fn[0], 2.2f, 1e-4f);

  // One conversion to sRGB: (128/255)^2.2 -> sRGB encode is ~128.6.
  auto shown = skity::ColorSpace::Convert(
      space, skity::Color4f{128.f / 255.f, 128.f / 255.f, 128.f / 255.f, 1.f},
      nullptr);
  EXPECT_NEAR(shown.r * 255.f, 128.6f, 1.f);
}

TEST(PNGCodecTest, LinearPNGIsConvertedOnce) {
  // gAMA=100000 (gamma 1.0): the decoded bytes stay linear and the display
  // conversion runs exactly once, sRGB-encoding 64/255 into ~137.
  auto file = MakeGammaPNG(100000, 64);
  ASSERT_TRUE(file != nullptr);
  auto codec = skity::Codec::MakePngCodec();
  codec->SetData(file);
  auto decoded = codec->Decode();
  ASSERT_TRUE(decoded != nullptr);

  auto pixel = static_cast<const uint8_t*>(decoded->Addr());
  EXPECT_EQ(pixel[0], 64);

  auto space = decoded->GetColorSpace();
  ASSERT_TRUE(space != nullptr);
  EXPECT_NEAR(space->transfer_fn[0], 1.f, 1e-5f);

  auto shown = skity::ColorSpace::Convert(
      space, skity::Color4f{64.f / 255.f, 64.f / 255.f, 64.f / 255.f, 1.f},
      nullptr);
  EXPECT_NEAR(shown.r * 255.f, 137.f, 1.f);
}

TEST(PNGCodecTest, CorruptGammaChunkIsIgnored) {
  // A gAMA chunk with a broken CRC is discarded by libpng (ancillary
  // chunks: warn and discard), so the decode keeps the default sRGB tag
  // instead of applying metadata the decoder itself did not trust.
  auto file = MakeGammaPNG(45455, 128);
  ASSERT_TRUE(file != nullptr);
  auto corrupted = CorruptChunkCRC(file.get(), "gAMA");
  ASSERT_TRUE(corrupted != nullptr);
  ASSERT_EQ(FindGAMAValue(corrupted.get()), 45455u);

  auto codec = skity::Codec::MakePngCodec();
  codec->SetData(corrupted);
  auto decoded = codec->Decode();
  ASSERT_TRUE(decoded != nullptr);
  EXPECT_EQ(decoded->GetColorSpace(), nullptr);
  // The pixels themselves are unaffected.
  auto pixel = static_cast<const uint8_t*>(decoded->Addr());
  EXPECT_EQ(pixel[0], 128);
}

TEST(PNGCodecTest, ScaledDecodePreservesColorTag) {
  // A P3 solid color decodes to the same display color whether decoded at
  // intrinsic size or scaled down: the resampled pixmap keeps the source
  // tag instead of silently falling back to sRGB.
  auto p3 = skity::ColorSpace::DisplayP3();
  skity::Pixmap pixmap(8, 8, skity::kUnpremul_AlphaType,
                       skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap.SetColorInfo(skity::kUnpremul_AlphaType,
                                  skity::ColorType::kRGBA, p3));
  for (uint32_t y = 0; y < 8; y++) {
    for (uint32_t x = 0; x < 8; x++) {
      auto addr = pixmap.WritableAddr8(x, y);
      addr[0] = 192;
      addr[1] = 64;
      addr[2] = 64;
      addr[3] = 255;
    }
  }
  auto codec = skity::Codec::MakePngCodec();
  auto data = codec->Encode(&pixmap);
  ASSERT_TRUE(data != nullptr);

  codec->SetData(data);
  auto full = codec->Decode();
  ASSERT_TRUE(full != nullptr);
  ASSERT_TRUE(full->GetColorSpace() != nullptr);

  skity::DecodeOptions options;
  options.target_width = 1;
  options.target_height = 1;
  auto scaled = codec->Decode(options);
  ASSERT_TRUE(scaled != nullptr);
  ASSERT_TRUE(scaled->GetColorSpace() != nullptr);
  EXPECT_TRUE(
      skity::ColorSpaceEquals(scaled->GetColorSpace(), full->GetColorSpace()));

  // Same source encoding, same conversion: the display color matches.
  auto full_pixel = static_cast<const uint8_t*>(full->Addr());
  auto scaled_pixel = static_cast<const uint8_t*>(scaled->Addr());
  auto shown_full = skity::ColorSpace::Convert(
      full->GetColorSpace(),
      skity::Color4f{full_pixel[0] / 255.f, full_pixel[1] / 255.f,
                     full_pixel[2] / 255.f, 1.f},
      nullptr);
  auto shown_scaled = skity::ColorSpace::Convert(
      scaled->GetColorSpace(),
      skity::Color4f{scaled_pixel[0] / 255.f, scaled_pixel[1] / 255.f,
                     scaled_pixel[2] / 255.f, 1.f},
      nullptr);
  EXPECT_NEAR(shown_full.r, shown_scaled.r, 2.f / 255.f);
  EXPECT_NEAR(shown_full.g, shown_scaled.g, 2.f / 255.f);
  EXPECT_NEAR(shown_full.b, shown_scaled.b, 2.f / 255.f);
}

namespace {

uint32_t Adler32(const uint8_t* data, size_t size) {
  uint32_t a = 1;
  uint32_t b = 0;
  for (size_t i = 0; i < size; i++) {
    a = (a + data[i]) % 65521u;
    b = (b + a) % 65521u;
  }
  return (b << 16) | a;
}

// Hand-builds a 1x1 16-bit RGB PNG (no color chunks) carrying the given
// big-endian samples. The zlib stream uses stored (uncompressed) deflate
// blocks, so the bytes are fixed without a compressor, letting the test
// choose samples where scaling and truncation disagree.
std::shared_ptr<skity::Data> MakeSixteenBitPNG(uint16_t r, uint16_t g,
                                               uint16_t b) {
  // Scanline: filter byte + one 16-bit RGB pixel.
  uint8_t scanline[7] = {0,
                         static_cast<uint8_t>(r >> 8),
                         static_cast<uint8_t>(r),
                         static_cast<uint8_t>(g >> 8),
                         static_cast<uint8_t>(g),
                         static_cast<uint8_t>(b >> 8),
                         static_cast<uint8_t>(b)};
  // zlib header (0x78 0x01) + one final stored block + adler32.
  std::vector<uint8_t> idat = {0x78, 0x01, 0x01, 0x07, 0x00, 0xF8, 0xFF};
  idat.insert(idat.end(), scanline, scanline + sizeof(scanline));
  uint32_t adler = Adler32(scanline, sizeof(scanline));
  idat.push_back(static_cast<uint8_t>(adler >> 24));
  idat.push_back(static_cast<uint8_t>(adler >> 16));
  idat.push_back(static_cast<uint8_t>(adler >> 8));
  idat.push_back(static_cast<uint8_t>(adler));

  uint8_t ihdr[13] = {0, 0, 0, 1, 0, 0, 0, 1, 16, 2, 0, 0, 0};
  const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

  std::vector<uint8_t> out(signature, signature + 8);
  auto append_chunk = [&](const char type[5], const uint8_t* payload,
                          size_t len) {
    out.push_back(static_cast<uint8_t>(len >> 24));
    out.push_back(static_cast<uint8_t>(len >> 16));
    out.push_back(static_cast<uint8_t>(len >> 8));
    out.push_back(static_cast<uint8_t>(len));
    size_t crc_start = out.size();
    out.insert(out.end(), type, type + 4);
    if (len > 0) {
      out.insert(out.end(), payload, payload + len);
    }
    uint32_t crc = Crc32(out.data() + crc_start, out.size() - crc_start);
    out.push_back(static_cast<uint8_t>(crc >> 24));
    out.push_back(static_cast<uint8_t>(crc >> 16));
    out.push_back(static_cast<uint8_t>(crc >> 8));
    out.push_back(static_cast<uint8_t>(crc));
  };
  append_chunk("IHDR", ihdr, sizeof(ihdr));
  append_chunk("IDAT", idat.data(), idat.size());
  append_chunk("IEND", nullptr, 0);
  return skity::Data::MakeWithCopy(out.data(), out.size());
}

}  // namespace

TEST(PNGCodecTest, SixteenBitPNGScalesInsteadOfStrips) {
  // 16-bit -> 8-bit must scale (round(V / 257)), not truncate the low byte:
  // the low byte carries real precision (0x00FF is ~1/255, not 0). The
  // samples are chosen so scaling and truncation disagree on two channels.
  auto file = MakeSixteenBitPNG(0x00FF, 0xFF00, 0x8000);
  ASSERT_TRUE(file != nullptr);
  auto codec = skity::Codec::MakePngCodec();
  ASSERT_TRUE(codec->RecognizeFileType(
      reinterpret_cast<const char*>(file->RawData()), file->Size()));

  codec->SetData(file);
  auto decoded = codec->Decode();
  ASSERT_TRUE(decoded != nullptr);
  EXPECT_EQ(decoded->Width(), 1);
  EXPECT_EQ(decoded->Height(), 1);
  // No color chunks in the file: the decode stays undeclared (sRGB).
  EXPECT_EQ(decoded->GetColorSpace(), nullptr);

  auto pixel = static_cast<const uint8_t*>(decoded->Addr());
  EXPECT_EQ(pixel[0], 1);    // 0x00FF -> 1; truncation would give 0.
  EXPECT_EQ(pixel[1], 254);  // 0xFF00 -> 254; truncation would give 255.
  EXPECT_EQ(pixel[2], 128);  // 0x8000 -> 128 either way.
  EXPECT_EQ(pixel[3], 255);  // filler alpha.
}

TEST(PNGCodecBusinessReview, InvalidIdatReleasesDecodedBuffer) {
#if defined(__APPLE__)
  skity::Pixmap pixmap(512, 512, skity::kUnpremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto codec = skity::Codec::MakePngCodec();
  auto file = codec->Encode(&pixmap);
  ASSERT_NE(file, nullptr);
  auto invalid = CorruptChunkCRC(file.get(), "IDAT");
  ASSERT_NE(invalid, nullptr);
  codec->SetData(invalid);
  EXPECT_EQ(codec->Decode(), nullptr);  // Warm up allocator and error handling.
  malloc_statistics_t before{}, after{};
  malloc_zone_statistics(nullptr, &before);
  constexpr int kAttempts = 12;
  for (int i = 0; i < kAttempts; ++i) {
    EXPECT_EQ(codec->Decode(), nullptr);
  }
  malloc_zone_statistics(nullptr, &after);
  const auto growth = static_cast<long long>(after.size_in_use) -
                      static_cast<long long>(before.size_in_use);
  RecordProperty("live_heap_growth_bytes", growth);
  EXPECT_LT(growth, 1024 * 1024)
      << "live heap growth after " << kAttempts << " failed 512x512 decodes";
#else
  GTEST_SKIP() << "This heap accounting check requires macOS; use a leak "
                  "detector on other platforms.";
#endif
}

namespace {
// The valid Display P3 ICC fixture is embedded so this test needs no
// macOS profile installation or external fixture path.
const uint8_t kBusinessReviewP3IccChunk[] = {
    68,  105, 115, 112, 108, 97,  121, 32,  80,  51,  0,   0,   120, 156, 125,
    144, 177, 75,  195, 80,  16,  198, 191, 86,  165, 160, 117, 16,  29,  28,
    28,  50,  137, 67,  148, 146, 10,  186, 56,  180, 21,  68,  113, 8,   85,
    193, 234, 148, 190, 166, 169, 144, 198, 71,  146, 34,  5,   55,  255, 129,
    130, 255, 129, 10,  206, 110, 22,  135, 58,  58,  56,  8,   162, 147, 232,
    230, 228, 164, 224, 162, 229, 121, 47,  137, 164, 34,  122, 143, 227, 126,
    124, 239, 187, 227, 56,  32,  57,  110, 112, 110, 247, 3,   168, 59,  190,
    91,  92,  202, 43,  155, 165, 45,  37,  245, 140, 4,   189, 32,  12,  230,
    241, 156, 174, 175, 74,  254, 174, 63,  227, 253, 62,  244, 222, 78,  203,
    89,  191, 255, 255, 141, 193, 138, 233, 49,  170, 159, 148, 25,  198, 93,
    31,  72,  168, 196, 250, 158, 207, 37,  239, 19,  143, 185, 180, 20,  113,
    75,  178, 21,  242, 137, 228, 114, 200, 231, 129, 103, 189, 88,  32,  190,
    38,  86,  88,  205, 168, 16,  191, 16,  171, 229, 30,  221, 234, 225, 186,
    221, 96,  209, 14,  114, 251, 180, 233, 108, 172, 201, 57,  148, 19,  88,
    196, 14,  60,  112, 216, 48,  208, 132, 2,   29,  217, 63,  252, 179, 129,
    191, 128, 93,  114, 55,  225, 82,  159, 133, 26,  124, 234, 201, 145, 34,
    39,  152, 196, 203, 112, 192, 48,  3,   149, 88,  67,  134, 82,  147, 119,
    142, 238, 119, 23,  221, 79,  141, 181, 131, 39,  96,  161, 35,  132, 184,
    136, 181, 149, 14,  112, 54,  71,  39,  107, 199, 218, 212, 60,  48,  50,
    4,   92,  181, 185, 225, 26,  129, 212, 71,  153, 172, 86,  129, 215, 83,
    96,  184, 4,   140, 222, 80,  207, 182, 87,  205, 106, 225, 246, 233, 60,
    48,  240, 40,  196, 219, 36,  144, 58,  4,   186, 45,  33,  62,  142, 132,
    232, 30,  83,  243, 3,   112, 233, 124, 1,   3,   167, 98,  19,
};
}  // namespace

TEST(PNGCodecBusinessReview, UnsupportedIccStillUsesFallbackChunks) {
  const float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  auto space =
      skity::ColorSpace::MakeRGB(tf, skity::ColorSpace::DisplayP3()->gamut_xyz);
  ASSERT_NE(space, nullptr);
  skity::Pixmap pixmap(1, 1, skity::kUnpremul_AlphaType,
                       skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap.SetColorInfo(skity::kUnpremul_AlphaType,
                                  skity::ColorType::kRGBA, space));
  auto codec = skity::Codec::MakePngCodec();
  auto fallback = codec->Encode(&pixmap);
  ASSERT_NE(fallback, nullptr);
  codec->SetData(fallback);
  auto without_icc = codec->Decode();
  ASSERT_NE(without_icc, nullptr);
  ASSERT_NE(without_icc->GetColorSpace(), nullptr);
  auto with_icc =
      InsertChunkBeforeIDAT(fallback.get(), "iCCP", kBusinessReviewP3IccChunk,
                            sizeof(kBusinessReviewP3IccChunk));
  ASSERT_NE(with_icc, nullptr);
  codec->SetData(with_icc);
  auto decoded = codec->Decode();
  ASSERT_NE(decoded, nullptr);
  RecordProperty("fallback_became_srgb",
                 skity::ColorSpaceIsSRGB(decoded->GetColorSpace()) ? 1 : 0);
  ASSERT_NE(decoded->GetColorSpace(), nullptr)
      << "Unsupported ICC should not discard supported gAMA/cHRM fallbacks.";
  for (int i = 0; i < 9; ++i) {
    EXPECT_NEAR(decoded->GetColorSpace()->gamut_xyz[i],
                without_icc->GetColorSpace()->gamut_xyz[i], 1e-5f);
  }
}

TEST(PNGCodecBusinessReview, IccOnlyFileStaysSRGB) {
  // An iCCP the decoder cannot read only matters through the approximate
  // chunks beside it: with no gAMA/cHRM to fall back to, the file is as
  // undeclared as one without any color description.
  skity::Pixmap pixmap(1, 1, skity::kUnpremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto addr = pixmap.WritableAddr8(0, 0);
  addr[0] = addr[1] = addr[2] = 128;
  addr[3] = 255;
  auto codec = skity::Codec::MakePngCodec();
  auto data = codec->Encode(&pixmap);
  ASSERT_NE(data, nullptr);
  auto with_icc =
      InsertChunkBeforeIDAT(data.get(), "iCCP", kBusinessReviewP3IccChunk,
                            sizeof(kBusinessReviewP3IccChunk));
  ASSERT_NE(with_icc, nullptr);

  codec->SetData(with_icc);
  auto decoded = codec->Decode();
  ASSERT_NE(decoded, nullptr);
  EXPECT_EQ(decoded->GetColorSpace(), nullptr);
  EXPECT_TRUE(skity::ColorSpaceIsSRGB(decoded->GetColorSpace()));
  // The pixels themselves are unaffected either way.
  auto pixel = static_cast<const uint8_t*>(decoded->Addr());
  EXPECT_EQ(pixel[0], 128);
}
