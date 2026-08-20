// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <skity/codec/codec.hpp>
#include <skity/graphic/color.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>

#include "module/codec/src/codec/codec_priv.hpp"

namespace {

using skity::DecodeOptions;
using skity::codec_priv::ResamplePixmap;
using skity::codec_priv::ResolveTargetSize;

std::shared_ptr<skity::Pixmap> MakeSolidPixmap(uint32_t width, uint32_t height,
                                               uint8_t r, uint8_t g, uint8_t b,
                                               uint8_t a) {
  auto pixmap = std::make_shared<skity::Pixmap>(
      width, height, skity::AlphaType::kUnpremul_AlphaType,
      skity::ColorType::kRGBA);

  for (uint32_t y = 0; y < height; y++) {
    uint8_t* row = pixmap->WritableAddr8(0, y);
    for (uint32_t x = 0; x < width; x++) {
      row[x * 4 + 0] = r;
      row[x * 4 + 1] = g;
      row[x * 4 + 2] = b;
      row[x * 4 + 3] = a;
    }
  }

  return pixmap;
}

// Fills a pixmap with alternating vertical stripes of two colors.
std::shared_ptr<skity::Pixmap> MakeStripedPixmap(uint32_t width,
                                                 uint32_t height,
                                                 const uint8_t color_a[4],
                                                 const uint8_t color_b[4]) {
  auto pixmap = std::make_shared<skity::Pixmap>(
      width, height, skity::AlphaType::kUnpremul_AlphaType,
      skity::ColorType::kRGBA);

  for (uint32_t y = 0; y < height; y++) {
    uint8_t* row = pixmap->WritableAddr8(0, y);
    for (uint32_t x = 0; x < width; x++) {
      const uint8_t* c = (x % 2 == 0) ? color_a : color_b;
      row[x * 4 + 0] = c[0];
      row[x * 4 + 1] = c[1];
      row[x * 4 + 2] = c[2];
      row[x * 4 + 3] = c[3];
    }
  }

  return pixmap;
}

void ExpectPixelNear(const skity::Pixmap& pixmap, uint32_t x, uint32_t y,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                     int tolerance) {
  const uint8_t* p = pixmap.Addr8(x, y);
  EXPECT_NEAR(p[0], r, tolerance) << "red at " << x << "," << y;
  EXPECT_NEAR(p[1], g, tolerance) << "green at " << x << "," << y;
  EXPECT_NEAR(p[2], b, tolerance) << "blue at " << x << "," << y;
  EXPECT_NEAR(p[3], a, tolerance) << "alpha at " << x << "," << y;
}

}  // namespace

TEST(CodecScaleTest, ResolveTargetSize) {
  int32_t out_width = 0;
  int32_t out_height = 0;

  // No target requested: intrinsic size.
  DecodeOptions none{};
  EXPECT_FALSE(ResolveTargetSize(100, 200, none, &out_width, &out_height));

  // Aspect-fit box.
  DecodeOptions box{};
  box.target_width = 80;
  box.target_height = 80;
  EXPECT_TRUE(ResolveTargetSize(100, 200, box, &out_width, &out_height));
  EXPECT_EQ(out_width, 40);
  EXPECT_EQ(out_height, 80);

  // One dimension unspecified: derived from the other.
  DecodeOptions height_only{};
  height_only.target_height = 50;
  EXPECT_TRUE(
      ResolveTargetSize(100, 200, height_only, &out_width, &out_height));
  EXPECT_EQ(out_width, 25);
  EXPECT_EQ(out_height, 50);

  DecodeOptions width_only{};
  width_only.target_width = 50;
  EXPECT_TRUE(ResolveTargetSize(100, 200, width_only, &out_width, &out_height));
  EXPECT_EQ(out_width, 50);
  EXPECT_EQ(out_height, 100);

  // Never upscale: box already contains the source.
  DecodeOptions large{};
  large.target_width = 200;
  large.target_height = 300;
  EXPECT_FALSE(ResolveTargetSize(100, 200, large, &out_width, &out_height));

  // Exact intrinsic box: no scaling either.
  DecodeOptions exact{};
  exact.target_width = 100;
  exact.target_height = 200;
  EXPECT_FALSE(ResolveTargetSize(100, 200, exact, &out_width, &out_height));

  // Mixed: one dimension larger, one smaller — the smaller one wins.
  DecodeOptions mixed{};
  mixed.target_width = 200;
  mixed.target_height = 100;
  EXPECT_TRUE(ResolveTargetSize(100, 200, mixed, &out_width, &out_height));
  EXPECT_EQ(out_width, 50);
  EXPECT_EQ(out_height, 100);

  // Negative targets are treated as unspecified.
  DecodeOptions negative{};
  negative.target_width = -10;
  negative.target_height = 50;
  EXPECT_TRUE(ResolveTargetSize(100, 200, negative, &out_width, &out_height));
  EXPECT_EQ(out_width, 25);
  EXPECT_EQ(out_height, 50);

  // Degenerate source.
  EXPECT_FALSE(ResolveTargetSize(0, 100, box, &out_width, &out_height));

  // Tiny box: scale is clamped by the wider dimension, sizes floor at 1.
  DecodeOptions tiny{};
  tiny.target_width = 1;
  tiny.target_height = 1;
  EXPECT_TRUE(ResolveTargetSize(100, 200, tiny, &out_width, &out_height));
  EXPECT_EQ(out_width, 1);
  EXPECT_EQ(out_height, 1);
}

TEST(CodecScaleTest, ResampleRejectsInvalidInput) {
  auto solid = MakeSolidPixmap(4, 4, 1, 2, 3, 255);

  EXPECT_TRUE(ResamplePixmap(nullptr, 2, 2) == nullptr);
  EXPECT_TRUE(ResamplePixmap(solid, 0, 2) == nullptr);
  EXPECT_TRUE(ResamplePixmap(solid, 2, -1) == nullptr);

  // Non-canonical color info is refused rather than resampled as garbage.
  auto bgra = std::make_shared<skity::Pixmap>(
      4, 4, skity::AlphaType::kUnpremul_AlphaType, skity::ColorType::kBGRA);
  EXPECT_TRUE(ResamplePixmap(bgra, 2, 2) == nullptr);
}

TEST(CodecScaleTest, ResampleSolidColor) {
  auto src = MakeSolidPixmap(8, 6, 10, 20, 30, 255);
  auto dst = ResamplePixmap(src, 3, 2);

  ASSERT_TRUE(dst != nullptr);
  EXPECT_EQ(dst->Width(), 3u);
  EXPECT_EQ(dst->Height(), 2u);
  EXPECT_EQ(dst->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(dst->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);

  for (uint32_t y = 0; y < dst->Height(); y++) {
    for (uint32_t x = 0; x < dst->Width(); x++) {
      ExpectPixelNear(*dst, x, y, 10, 20, 30, 255, 0);
    }
  }
}

TEST(CodecScaleTest, ResampleExactAverage) {
  // 2x2 with left column red and right column blue: the 1x1 area average of
  // opaque red and opaque blue is (128, 0, 128, 255).
  const uint8_t red[4] = {255, 0, 0, 255};
  const uint8_t blue[4] = {0, 0, 255, 255};
  auto src = MakeStripedPixmap(2, 2, red, blue);

  auto dst = ResamplePixmap(src, 1, 1);
  ASSERT_TRUE(dst != nullptr);
  EXPECT_EQ(dst->Width(), 1u);
  EXPECT_EQ(dst->Height(), 1u);
  ExpectPixelNear(*dst, 0, 0, 128, 0, 128, 255, 0);
}

TEST(CodecScaleTest, ResampleFiltersInPremulSpace) {
  // Opaque red next to fully transparent: filtering straight (unpremultiplied)
  // RGBA would bleed blue into the result as (128, 0, 128, 128); filtering in
  // premultiplied space yields (255, 0, 0, 128).
  auto src = MakeSolidPixmap(2, 1, 255, 0, 0, 255);
  {
    uint8_t* row = src->WritableAddr8(0, 0);
    row[4 + 0] = 0;
    row[4 + 1] = 0;
    row[4 + 2] = 255;
    row[4 + 3] = 0;
  }

  auto dst = ResamplePixmap(src, 1, 1);
  ASSERT_TRUE(dst != nullptr);
  ExpectPixelNear(*dst, 0, 0, 255, 0, 0, 128, 1);
}

TEST(CodecScaleTest, ResampleIdentityReturnsInput) {
  auto src = MakeSolidPixmap(4, 4, 1, 2, 3, 255);
  auto dst = ResamplePixmap(src, 4, 4);
  EXPECT_EQ(dst.get(), src.get());
}

TEST(CodecScaleTest, JPEGDecodeScaled) {
  auto jpeg_data = skity::Data::MakeFromFileName(SKITY_TEST_JPEG_FILE);
  ASSERT_TRUE(jpeg_data != nullptr);
  // image1.jpg is 133x100.

  auto codec = skity::Codec::MakeFromData(jpeg_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(jpeg_data);

  // Scale 0.5 lands between n/8 ratios (3/8); the remainder pass must bring
  // the output to the exact aspect-fit target.
  DecodeOptions half{};
  half.target_width = 66;
  half.target_height = 50;
  auto pixmap = codec->Decode(half);
  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 66u);
  EXPECT_EQ(pixmap->Height(), 50u);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);

  // One dimension unspecified.
  DecodeOptions width_only{};
  width_only.target_width = 66;
  pixmap = codec->Decode(width_only);
  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 66u);
  EXPECT_EQ(pixmap->Height(), 50u);

  // Never upscale.
  DecodeOptions huge{};
  huge.target_width = 400;
  huge.target_height = 400;
  pixmap = codec->Decode(huge);
  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 133u);
  EXPECT_EQ(pixmap->Height(), 100u);
}

// Fixed-seed LCG noise: every pixel uncorrelated, so the output's
// row-difference energy directly measures how much detail survived the
// decode-time scaling (see the JPEG quality test below).
std::shared_ptr<skity::Pixmap> MakeNoisePixmap(uint32_t width,
                                               uint32_t height) {
  auto pixmap = std::make_shared<skity::Pixmap>(
      width, height, skity::AlphaType::kUnpremul_AlphaType,
      skity::ColorType::kRGBA);
  uint32_t state = 0x12345678u;
  for (uint32_t y = 0; y < height; y++) {
    uint8_t* row = pixmap->WritableAddr8(0, y);
    for (uint32_t x = 0; x < width; x++) {
      state = state * 1664525u + 1013904223u;
      uint8_t v = static_cast<uint8_t>((state >> 16) & 0xFF);
      row[x * 4 + 0] = v;
      row[x * 4 + 1] = v;
      row[x * 4 + 2] = v;
      row[x * 4 + 3] = 255;
    }
  }
  return pixmap;
}

TEST(CodecScaleTest, JPEGDecodeScaledHighFrequencyQuality) {
  // Noise content scaled just below an n/8 grid point (0.4922, i.e. under
  // 4/8): the scaled decode must stay close to the "full decode + shared
  // resampler" reference and must keep the detail level of a downscaled
  // decode. The pre-fix floor rule picked 3/8 (96x96 native) and grew it
  // 31% to the target; interpolated growth carries systematically less
  // detail per output row than the covering rule's 4/8 (128x128) decode
  // with a 1.5% downscale remainder.
  auto noisy = MakeNoisePixmap(256, 256);

  auto encoder = skity::Codec::MakeJPEGCodec();
  auto jpeg_data = encoder->Encode(noisy.get());
  ASSERT_TRUE(jpeg_data != nullptr);

  auto codec = skity::Codec::MakeFromData(jpeg_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(jpeg_data);

  // Reference: full decode, then the shared resampler.
  auto full = codec->Decode();
  ASSERT_TRUE(full != nullptr);
  auto reference = ResamplePixmap(full, 126, 126);
  ASSERT_TRUE(reference != nullptr);

  // Scaled decode: scale 0.4922 sits just below the 4/8 grid point, the
  // worst case for the floor rule.
  DecodeOptions box{};
  box.target_width = 126;
  box.target_height = 126;
  auto scaled = codec->Decode(box);
  ASSERT_TRUE(scaled != nullptr);
  ASSERT_EQ(scaled->Width(), 126u);
  ASSERT_EQ(scaled->Height(), 126u);

  int64_t total_err = 0;
  for (uint32_t y = 0; y < 126; y++) {
    for (uint32_t x = 0; x < 126; x++) {
      const uint8_t* ref_px = reference->Addr8(x, y);
      const uint8_t* got_px = scaled->Addr8(x, y);
      for (int c = 0; c < 3; c++) {
        total_err += std::abs(static_cast<int32_t>(ref_px[c]) -
                              static_cast<int32_t>(got_px[c]));
      }
    }
  }
  double mean_err = static_cast<double>(total_err) / (126.0 * 126.0 * 3.0);

  // Loose quality guard: the covering rule measures mean ~13.7 here (the
  // 1/2 IDCT resample against the full-decode reference); a regression that
  // discards much more detail lands well above this bound.
  EXPECT_LT(mean_err, 20.0);

  // The precise upscale detector: the output must keep a fixed fraction of
  // the reference's vertical detail (row-difference energy). The covering
  // rule measures ratio ~0.71; the pre-fix floor rule (decode 96x96, grow
  // 31% to the target) measures ~0.53 — interpolation-inflated output has
  // systematically less detail per row than a downscaled decode. Being a
  // ratio against the same-pipeline reference, the bound is robust to
  // libjpeg IDCT flavor differences across platforms.
  auto row_energy = [](const skity::Pixmap& pm) {
    int64_t e = 0;
    for (uint32_t y = 1; y < pm.Height(); y++) {
      for (uint32_t x = 0; x < pm.Width(); x++) {
        const uint8_t* a = pm.Addr8(x, y);
        const uint8_t* b = pm.Addr8(x, y - 1);
        for (int c = 0; c < 3; c++) {
          e +=
              std::abs(static_cast<int32_t>(a[c]) - static_cast<int32_t>(b[c]));
        }
      }
    }
    return e;
  };
  double detail_ratio = static_cast<double>(row_energy(*scaled)) /
                        static_cast<double>(row_energy(*reference));
  EXPECT_GT(detail_ratio, 0.62);
}

TEST(CodecScaleTest, JPEGDecodeScaledNearIntrinsic) {
  // Targets within 1/8 of the intrinsic size (scale in (7/8, 1)) round up
  // to the 8/8 IDCT level: no scaled decode happens, but the remainder pass
  // must still bring the output below the requested box. The pre-fix code
  // skipped the resample at 8/8 and returned the intrinsic 133x100, which
  // exceeds both boxes below.
  auto jpeg_data = skity::Data::MakeFromFileName(SKITY_TEST_JPEG_FILE);
  ASSERT_TRUE(jpeg_data != nullptr);

  auto codec = skity::Codec::MakeFromData(jpeg_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(jpeg_data);

  // Reviewer's repro: scale 0.99, just under intrinsic.
  DecodeOptions near_intrinsic{};
  near_intrinsic.target_width = 132;
  near_intrinsic.target_height = 99;
  auto pixmap = codec->Decode(near_intrinsic);

  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 132u);
  EXPECT_EQ(pixmap->Height(), 99u);

  // Mid-interval: scale 0.96.
  DecodeOptions mid{};
  mid.target_width = 128;
  mid.target_height = 96;
  pixmap = codec->Decode(mid);

  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 128u);
  EXPECT_EQ(pixmap->Height(), 96u);
}

TEST(CodecScaleTest, JPEGDecodeScaledNativeRatio) {
  // Scale exactly 0.5: native DCT scaling hits 67x50 on its own (133 * 3/8
  // would be 50x38), no remainder pass needed, dimensions still exact.
  auto jpeg_data = skity::Data::MakeFromFileName(SKITY_TEST_JPEG_FILE);
  ASSERT_TRUE(jpeg_data != nullptr);

  auto codec = skity::Codec::MakeFromData(jpeg_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(jpeg_data);

  DecodeOptions options{};
  options.target_width = 67;
  options.target_height = 50;
  auto pixmap = codec->Decode(options);

  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 67u);
  EXPECT_EQ(pixmap->Height(), 50u);
}

TEST(CodecScaleTest, JPEGDecodeScaledContent) {
  // A solid-color JPEG scaled to half size must stay that color within JPEG
  // codec tolerance.
  auto solid = MakeSolidPixmap(64, 64, 200, 30, 240, 255);

  auto encoder = skity::Codec::MakeJPEGCodec();
  auto jpeg_data = encoder->Encode(solid.get());
  ASSERT_TRUE(jpeg_data != nullptr);

  auto codec = skity::Codec::MakeFromData(jpeg_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(jpeg_data);

  DecodeOptions half{};

  auto full = codec->Decode(half);
  ASSERT_TRUE(full != nullptr);
  EXPECT_EQ(full->Width(), 64u);
  EXPECT_EQ(full->Height(), 64u);

  half.target_width = 32;
  half.target_height = 32;
  auto scaled = codec->Decode(half);
  ASSERT_TRUE(scaled != nullptr);
  EXPECT_EQ(scaled->Width(), 32u);
  EXPECT_EQ(scaled->Height(), 32u);

  for (uint32_t y = 0; y < scaled->Height(); y++) {
    for (uint32_t x = 0; x < scaled->Width(); x++) {
      ExpectPixelNear(*scaled, x, y, 200, 30, 240, 255, 16);
    }
  }
}

TEST(CodecScaleTest, JPEGDecodeScaledCorrupted) {
  // Truncated progressive JPEG (declared 303x455): the partial decode must
  // still come back at the requested scaled size.
  auto jpeg_data = skity::Data::MakeFromFileName(SKITY_TEST_CORRUPT_JPEG_FILE);
  ASSERT_TRUE(jpeg_data != nullptr);

  auto codec = skity::Codec::MakeFromData(jpeg_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(jpeg_data);

  DecodeOptions box{};
  box.target_width = 100;
  box.target_height = 100;
  auto pixmap = codec->Decode(box);

  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 67u);
  EXPECT_EQ(pixmap->Height(), 100u);
}

TEST(CodecScaleTest, PNGDecodeScaled) {
  // firefox_64.png is 64x64; PNG has no native scaling, the shared resampler
  // produces the target.
  auto png_data = skity::Data::MakeFromFileName(SKITY_TEST_PNG_FILE);
  ASSERT_TRUE(png_data != nullptr);

  auto codec = skity::Codec::MakeFromData(png_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(png_data);

  DecodeOptions quarter{};
  quarter.target_width = 32;
  quarter.target_height = 32;
  auto pixmap = codec->Decode(quarter);

  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 32u);
  EXPECT_EQ(pixmap->Height(), 32u);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);

  // Never upscale.
  DecodeOptions huge{};
  huge.target_width = 128;
  huge.target_height = 128;
  pixmap = codec->Decode(huge);
  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 64u);
  EXPECT_EQ(pixmap->Height(), 64u);
}

TEST(CodecScaleTest, PNGDecodeScaledContent) {
  // Solid semi-transparent red keeps its exact unpremultiplied color through
  // the premul-space resampler round trip.
  auto solid = MakeSolidPixmap(64, 64, 255, 0, 0, 128);

  auto encoder = skity::Codec::MakePngCodec();
  auto png_data = encoder->Encode(solid.get());
  ASSERT_TRUE(png_data != nullptr);

  auto codec = skity::Codec::MakeFromData(png_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(png_data);

  DecodeOptions half{};
  half.target_width = 32;
  half.target_height = 32;
  auto scaled = codec->Decode(half);

  ASSERT_TRUE(scaled != nullptr);
  EXPECT_EQ(scaled->Width(), 32u);
  EXPECT_EQ(scaled->Height(), 32u);

  for (uint32_t y = 0; y < scaled->Height(); y++) {
    for (uint32_t x = 0; x < scaled->Width(); x++) {
      ExpectPixelNear(*scaled, x, y, 255, 0, 0, 128, 1);
    }
  }
}

TEST(CodecScaleTest, BMPDecodeScaled) {
  // BMP has no native scaling; encode a solid 64x64 image and decode it at
  // half size through the shared resampler. A minimal 1x1 24-bit header is
  // enough to resolve MakeFromData to the BMP codec for encoding.
  const unsigned char bmp_header[] = {
      'B',  'M',  0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36,
      0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x04, 0x00, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x13, 0x0B,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  auto solid = MakeSolidPixmap(64, 64, 10, 200, 30, 255);

  auto encoder = skity::Codec::MakeFromData(
      skity::Data::MakeWithCopy(bmp_header, sizeof(bmp_header)));
  ASSERT_TRUE(encoder != nullptr);
  auto bmp_data = encoder->Encode(solid.get());
  ASSERT_TRUE(bmp_data != nullptr);

  auto codec = skity::Codec::MakeFromData(bmp_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(bmp_data);

  DecodeOptions half{};
  half.target_width = 32;
  half.target_height = 32;
  auto scaled = codec->Decode(half);

  ASSERT_TRUE(scaled != nullptr);
  EXPECT_EQ(scaled->Width(), 32u);
  EXPECT_EQ(scaled->Height(), 32u);
  EXPECT_EQ(scaled->GetColorType(), skity::ColorType::kRGBA);

  for (uint32_t y = 0; y < scaled->Height(); y++) {
    for (uint32_t x = 0; x < scaled->Width(); x++) {
      ExpectPixelNear(*scaled, x, y, 10, 200, 30, 255, 1);
    }
  }
}

TEST(CodecScaleTest, GIFDecodeScaled) {
  // Single-frame GIF (color wheel): intrinsic decode via wuffs, then
  // resampled to the target. Dimensions are asserted relative to the
  // intrinsic size to stay independent of the resource file.
  auto gif_data = skity::Data::MakeFromFileName(SKITY_TEST_SF_GIF_FILE);
  ASSERT_TRUE(gif_data != nullptr);

  auto codec = skity::Codec::MakeFromData(gif_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(gif_data);

  auto intrinsic = codec->Decode();
  ASSERT_TRUE(intrinsic != nullptr);
  ASSERT_GT(intrinsic->Width(), 4u);

  DecodeOptions half{};
  half.target_width = static_cast<int32_t>(intrinsic->Width()) / 2;
  half.target_height = static_cast<int32_t>(intrinsic->Height()) / 2;
  auto scaled = codec->Decode(half);

  ASSERT_TRUE(scaled != nullptr);
  EXPECT_EQ(scaled->Width(), static_cast<uint32_t>(half.target_width));
  EXPECT_EQ(scaled->Height(), static_cast<uint32_t>(half.target_height));
  EXPECT_EQ(scaled->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(scaled->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);
}

TEST(CodecScaleTest, WebPDecodeScaledSingleFrame) {
  // color_wheel.webp is a single-frame 128x128 lossless WebP; libwebp's
  // rescaler decodes straight to the requested size.
  auto webp_data = skity::Data::MakeFromFileName(SKITY_TEST_SF_WEBP_FILE);
  ASSERT_TRUE(webp_data != nullptr);

  auto codec = skity::Codec::MakeFromData(webp_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(webp_data);

  DecodeOptions odd{};
  odd.target_width = 48;
  odd.target_height = 48;
  auto pixmap = codec->Decode(odd);

  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 48u);
  EXPECT_EQ(pixmap->Height(), 48u);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(pixmap->GetAlphaType(), skity::AlphaType::kUnpremul_AlphaType);

  // One dimension unspecified, non-integer ratio.
  DecodeOptions width_only{};
  width_only.target_width = 50;
  pixmap = codec->Decode(width_only);
  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 50u);
  EXPECT_EQ(pixmap->Height(), 50u);

  // Never upscale.
  DecodeOptions huge{};
  huge.target_width = 256;
  huge.target_height = 256;
  pixmap = codec->Decode(huge);
  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 128u);
  EXPECT_EQ(pixmap->Height(), 128u);
}

TEST(CodecScaleTest, WebPDecodeScaledAnimated) {
  // blendBG.webp is a 7-frame 200x200 animation: the canvas decodes at
  // intrinsic size and the shared resampler brings it to the target.
  auto webp_data = skity::Data::MakeFromFileName(SKITY_TEST_WEBP_FILE);
  ASSERT_TRUE(webp_data != nullptr);

  auto codec = skity::Codec::MakeFromData(webp_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(webp_data);

  DecodeOptions half{};
  half.target_width = 100;
  half.target_height = 100;
  auto pixmap = codec->Decode(half);

  ASSERT_TRUE(pixmap != nullptr);
  EXPECT_EQ(pixmap->Width(), 100u);
  EXPECT_EQ(pixmap->Height(), 100u);
  EXPECT_EQ(pixmap->GetColorType(), skity::ColorType::kRGBA);
}

TEST(CodecScaleTest, WebPDecodeScaledSingleFrameAnimation) {
  // single_frame_anim.webp is a legal animation with exactly one ANMF frame:
  // canvas 200x200, frame 128x128 at offset (40,60). frame_count alone cannot
  // distinguish it from a static WebP — it must composite the fragment onto
  // the canvas, not decode the fragment scaled to the canvas-derived target.
  auto webp_data =
      skity::Data::MakeFromFileName(SKITY_TEST_SINGLE_ANIM_WEBP_FILE);
  ASSERT_TRUE(webp_data != nullptr);

  auto codec = skity::Codec::MakeFromData(webp_data);
  ASSERT_TRUE(codec != nullptr);
  codec->SetData(webp_data);

  DecodeOptions half{};
  half.target_width = 100;
  half.target_height = 100;
  auto pixmap = codec->Decode(half);

  ASSERT_TRUE(pixmap != nullptr);
  // Canvas semantics: the 200x200 canvas composited with the frame at its
  // offset, scaled to 100x100.
  EXPECT_EQ(pixmap->Width(), 100u);
  EXPECT_EQ(pixmap->Height(), 100u);

  // The top-left corner is outside the frame rect (offset 40,60 scales to
  // 20,30): it must show the animation background (transparent), not frame
  // pixels — the pre-fix fast path stretched the fragment over the full
  // canvas and put opaque frame content here.
  const uint8_t* corner = pixmap->Addr8(0, 0);
  EXPECT_EQ(corner[3], 0);

  // Frame center (canvas 104,124 scales to 52,62) is opaque frame content.
  const uint8_t* center = pixmap->Addr8(52, 62);
  EXPECT_NE(center[3], 0);
}
