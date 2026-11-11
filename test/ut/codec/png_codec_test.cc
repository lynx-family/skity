// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

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

// Splices an sRGB chunk (perceptual intent) right before the first IDAT.
std::shared_ptr<skity::Data> InsertSRGBChunkBeforeIDAT(const skity::Data* png) {
  const uint8_t* bytes = static_cast<const uint8_t*>(png->RawData());
  size_t size = png->Size();
  size_t pos = 8;
  while (pos + 12 <= size) {
    uint32_t length = ReadBE32(bytes + pos);
    if (std::memcmp(bytes + pos + 4, "IDAT", 4) == 0) {
      std::vector<uint8_t> out(bytes, bytes + pos);
      const uint8_t chunk[] = {'s', 'R', 'G', 'B', 0};
      uint32_t crc = Crc32(chunk, sizeof(chunk));
      uint8_t header[4] = {0, 0, 0, 1};
      out.insert(out.end(), header, header + 4);
      out.insert(out.end(), chunk, chunk + sizeof(chunk));
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
  // gAMA stores gamma * 100000, so 2.2 comes back within 1e-5.
  EXPECT_NEAR(out_space.transfer_fn[0], 2.2f, 1e-5f);
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

  auto spliced = InsertSRGBChunkBeforeIDAT(data.get());
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
  EXPECT_NEAR(out_space.transfer_fn[0], 2.2f, 1e-5f);
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
  EXPECT_NEAR(out_space.transfer_fn[0], 2.2f, 1e-5f);
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
  EXPECT_NEAR(degraded_space->transfer_fn[0], 2.2f, 1e-5f);
  auto srgb = skity::ColorSpace::SRGB();
  EXPECT_EQ(std::memcmp(degraded_space->gamut_xyz, srgb->gamut_xyz,
                        sizeof(degraded_space->gamut_xyz)),
            0);
}

TEST(PNGCodecTest, GamutFromChromaticitiesMatchesDisplayP3) {
  // Direct check of the chromaticity solve: the P3 cHRM values reproduce
  // the Display P3 gamut table.
  float p3[8] = {0.3127f, 0.3290f, 0.680f, 0.320f,
                 0.265f,  0.690f,  0.150f, 0.060f};
  float gamut[9];
  ASSERT_TRUE(skity::codec_priv::CodecGamutFromChromaticities(p3, gamut));
  auto display_p3 = skity::ColorSpace::DisplayP3();
  for (int i = 0; i < 9; i++) {
    EXPECT_NEAR(gamut[i], display_p3->gamut_xyz[i], 2e-4f);
  }

  // x + y == 1 is legal (z == 0, like the P3 red primary itself).
  float zero_z[8] = {0.3127f, 0.3290f, 0.7f,   0.3f,
                     0.265f,  0.690f,  0.150f, 0.060f};
  EXPECT_TRUE(skity::codec_priv::CodecGamutFromChromaticities(zero_z, gamut));

  // Degenerate inputs are rejected: out of range, x + y > 1 and coincident
  // primaries (singular matrix).
  float bad_range[8] = {0.3127f, 0.3290f, 1.5f,   0.320f,
                        0.265f,  0.690f,  0.150f, 0.060f};
  EXPECT_FALSE(
      skity::codec_priv::CodecGamutFromChromaticities(bad_range, gamut));
  float bad_sum[8] = {0.3127f, 0.3290f, 0.9f,   0.9f,
                      0.265f,  0.690f,  0.150f, 0.060f};
  EXPECT_FALSE(skity::codec_priv::CodecGamutFromChromaticities(bad_sum, gamut));
  float singular[8] = {0.3127f, 0.3290f, 0.680f, 0.320f,
                       0.680f,  0.320f,  0.680f, 0.320f};
  EXPECT_FALSE(
      skity::codec_priv::CodecGamutFromChromaticities(singular, gamut));
}
TEST(PNGCodecTest, ChromaticitiesFromGamutUndoesTheAdaptation) {
  // The write-side inverse: the built-in Display P3 matrix (D50-adapted)
  // describes its own published chromaticities, and
  // gamut -> chromaticities -> gamut round trips.
  float c[8];
  auto p3 = skity::ColorSpace::DisplayP3();
  ASSERT_TRUE(
      skity::codec_priv::CodecChromaticitiesFromGamut(p3->gamut_xyz, c));
  EXPECT_NEAR(c[0], 0.3127, 1e-5);
  EXPECT_NEAR(c[1], 0.3290, 1e-5);
  EXPECT_NEAR(c[2], 0.680, 1e-5);
  EXPECT_NEAR(c[3], 0.320, 1e-5);
  EXPECT_NEAR(c[4], 0.265, 1e-5);
  EXPECT_NEAR(c[5], 0.690, 1e-5);
  EXPECT_NEAR(c[6], 0.150, 1e-5);
  EXPECT_NEAR(c[7], 0.060, 1e-5);
  float gamut[9];
  ASSERT_TRUE(skity::codec_priv::CodecGamutFromChromaticities(c, gamut));
  for (int i = 0; i < 9; i++) {
    EXPECT_NEAR(gamut[i], p3->gamut_xyz[i], 1e-5f);
  }

  // The sRGB matrix describes the published sRGB chromaticities; the 16-bit
  // rounding of its ICC constants bounds the accuracy.
  skity::ColorSpace srgb{};
  ASSERT_TRUE(
      skity::codec_priv::CodecChromaticitiesFromGamut(srgb.gamut_xyz, c));
  EXPECT_NEAR(c[2], 0.640, 2e-4);
  EXPECT_NEAR(c[3], 0.330, 2e-4);
  EXPECT_NEAR(c[4], 0.300, 2e-4);
  EXPECT_NEAR(c[5], 0.600, 2e-4);
  EXPECT_NEAR(c[6], 0.150, 2e-4);
  EXPECT_NEAR(c[7], 0.060, 2e-4);
}
