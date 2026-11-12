// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/bitmap_sampler.hpp"

#include <cmath>
#include <memory>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/render/canvas.hpp>

#include "gtest/gtest.h"

namespace {

float EncodeSRGB(float x) {
  return x <= 0.0031308f ? 12.92f * x
                         : 1.055f * std::pow(x, 1.f / 2.4f) - 0.055f;
}

}  // namespace

TEST(BitmapSamplerColorSpace, PremultipliedLinearSampling) {
  // Transfer functions are defined on unpremultiplied values. A premultiplied
  // linear sample (RGB=64, A=128) must convert through the unpremultiplied
  // domain and scale back: EncodeSRGB(64/128) * 128 is ~94, and no channel
  // may exceed the alpha after the conversion.
  auto pixmap = std::make_shared<skity::Pixmap>(1, 1, skity::kPremul_AlphaType,
                                                skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kPremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::SRGBLinear()));
  auto addr = pixmap->WritableAddr8(0, 0);
  addr[0] = addr[1] = addr[2] = 64;
  addr[3] = 128;
  skity::Bitmap bitmap(pixmap, false);

  skity::BitmapSampler sampler(bitmap, skity::SamplingOptions{},
                               skity::TileMode::kClamp,
                               skity::TileMode::kClamp);
  skity::Color color = sampler.GetColor(skity::Vec2{0.5f, 0.5f});

  EXPECT_EQ(ColorGetA(color), 128);
  EXPECT_NEAR(ColorGetR(color), EncodeSRGB(64.f / 128.f) * 128.f, 1.f);
  EXPECT_LE(ColorGetR(color), ColorGetA(color));
}

TEST(BitmapSamplerColorSpace, SoftwareNearestConvertsEntireSpan) {
  // A 9-pixel-wide linear gray bitmap drawn through the software canvas:
  // the span must convert uniformly. The ARM NEON fast path samples raw
  // texels, so without the scalar fallback the first 8 pixels stay
  // unconverted (64) while the remainder pixel converts (~137).
  auto pixmap = std::make_shared<skity::Pixmap>(
      9, 1, skity::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kUnpremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::SRGBLinear()));
  for (uint32_t x = 0; x < 9; x++) {
    auto addr = pixmap->WritableAddr8(x, 0);
    addr[0] = addr[1] = addr[2] = 64;
    addr[3] = 255;
  }
  skity::Bitmap bitmap(pixmap, false);

  skity::Bitmap output(9, 1, skity::kPremul_AlphaType);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&output);
  canvas->DrawImage(skity::Image::MakeImage(bitmap.GetPixmap()), 0.f, 0.f,
                    skity::SamplingOptions{skity::FilterMode::kNearest,
                                           skity::MipmapMode::kNone},
                    nullptr);

  // Every pixel converts the same way: EncodeSRGB(64/255) * 255 is ~137.
  auto expected = skity::ColorSetARGB(255, 137, 137, 137);
  for (uint32_t x = 0; x < 9; x++) {
    auto pixel = output.GetPixel(x, 0);
    // Allow +-1 for the conversion rounding against the fixed expectation.
    EXPECT_NEAR(ColorGetR(pixel), ColorGetR(expected), 1) << "pixel " << x;
  }
}

TEST(BitmapSamplerColorSpace, DecodesNonSRGBTaggedBitmap) {
  // Store an sRGB color pre-converted into Display P3 in a tagged bitmap;
  // sampling must decode it back to (approximately) the original color.
  skity::Color4f original{0.25f, 0.5f, 0.75f, 1.f};
  auto p3_color = skity::ColorSpace::Convert(
      skity::ColorSpace::SRGB(), original, skity::ColorSpace::DisplayP3());

  auto pixmap = std::make_shared<skity::Pixmap>(1, 1);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kUnpremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::DisplayP3()));
  skity::Bitmap bitmap(pixmap, false);
  bitmap.SetPixel(0, 0, skity::Color4fToColor(p3_color));

  skity::BitmapSampler sampler(bitmap, skity::SamplingOptions{},
                               skity::TileMode::kClamp,
                               skity::TileMode::kClamp);
  skity::Color decoded = sampler.GetColor(skity::Vec2{0.5f, 0.5f});

  // The decode matches running the same conversion on the stored value, and
  // both land back on the original sRGB color within 8-bit roundtrip error.
  auto expected = skity::Color4fFromColor(skity::Color4fToColor(p3_color));
  expected = skity::ColorSpace::Convert(skity::ColorSpace::DisplayP3(),
                                        expected, nullptr);
  EXPECT_EQ(decoded, skity::Color4fToColor(expected));
  auto decoded_4f = skity::Color4fFromColor(decoded);
  EXPECT_NEAR(decoded_4f.r, original.r, 2.f / 255.f);
  EXPECT_NEAR(decoded_4f.g, original.g, 2.f / 255.f);
  EXPECT_NEAR(decoded_4f.b, original.b, 2.f / 255.f);
  EXPECT_NEAR(decoded_4f.a, original.a, 2.f / 255.f);
}

TEST(BitmapSamplerColorSpace, SRGBBitmapIsUnchanged) {
  // Without a color space the sampler output must stay bit-identical.
  auto pixmap = std::make_shared<skity::Pixmap>(1, 1);
  skity::Bitmap bitmap(pixmap, false);
  auto stored = skity::ColorSetARGB(255, 30, 200, 160);
  bitmap.SetPixel(0, 0, stored);

  skity::BitmapSampler sampler(bitmap, skity::SamplingOptions{},
                               skity::TileMode::kClamp,
                               skity::TileMode::kClamp);
  EXPECT_EQ(sampler.GetColor(skity::Vec2{0.5f, 0.5f}), stored);
}

TEST(BitmapSamplerColorSpace, ZeroAlphaPremulSampleKeepsExactZeros) {
  // The premul path guards its divide: a zero-alpha sample must keep its
  // exact stored bytes instead of turning into NaNs.
  auto pixmap = std::make_shared<skity::Pixmap>(1, 1, skity::kPremul_AlphaType,
                                                skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kPremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::SRGBLinear()));
  auto addr = pixmap->WritableAddr8(0, 0);
  addr[0] = addr[1] = addr[2] = 0;
  addr[3] = 0;
  skity::Bitmap bitmap(pixmap, false);

  skity::BitmapSampler sampler(bitmap, skity::SamplingOptions{},
                               skity::TileMode::kClamp,
                               skity::TileMode::kClamp);
  EXPECT_EQ(sampler.GetColor(skity::Vec2{0.5f, 0.5f}),
            skity::ColorSetARGB(0, 0, 0, 0));
}

TEST(BitmapSamplerColorSpace, CubicSamplingStillConverts) {
  // The SW backend falls cubic sampling back to bilinear; the sampled value
  // must still decode through the tag (an early return on the cubic branch
  // would hand out the raw encoded bytes instead).
  auto pixmap = std::make_shared<skity::Pixmap>(
      1, 1, skity::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kUnpremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::SRGBLinear()));
  auto addr = pixmap->WritableAddr8(0, 0);
  addr[0] = addr[1] = addr[2] = 64;
  addr[3] = 255;
  skity::Bitmap bitmap(pixmap, false);

  skity::SamplingOptions options{};
  options.cubic = skity::CubicResampler{1.f / 3.f, 1.f / 3.f};
  skity::BitmapSampler sampler(bitmap, options, skity::TileMode::kClamp,
                               skity::TileMode::kClamp);
  skity::Color color = sampler.GetColor(skity::Vec2{0.5f, 0.5f});
  // EncodeSRGB(64/255) * 255 is ~137.
  EXPECT_NEAR(ColorGetR(color), 137, 1);
  EXPECT_NEAR(ColorGetG(color), 137, 1);
  EXPECT_NEAR(ColorGetB(color), 137, 1);
}
