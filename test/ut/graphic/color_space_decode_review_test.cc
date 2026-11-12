// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/graphic/image.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/render/canvas.hpp>

#include "src/graphic/bitmap_sampler.hpp"

namespace {

std::shared_ptr<skity::Pixmap> ReviewSolidPixmap(
    uint32_t width, skity::AlphaType alpha_type, skity::Color color,
    std::shared_ptr<const skity::ColorSpace> space) {
  auto pixels = std::make_shared<skity::Pixmap>(
      width, 1, alpha_type, skity::ColorType::kRGBA);
  pixels->SetColorInfo(alpha_type, skity::ColorType::kRGBA, space);
  skity::Bitmap bitmap(pixels, false);
  for (uint32_t x = 0; x < width; ++x) {
    bitmap.SetPixel(x, 0, color);
  }
  return pixels;
}

skity::Color DrawReviewPixmap(std::shared_ptr<skity::Pixmap> pixels,
                             skity::Color background, uint32_t x = 0) {
  skity::Bitmap output(pixels->Width(), 1, skity::kPremul_AlphaType);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&output);
  canvas->Clear(background);
  canvas->DrawImage(skity::Image::MakeImage(pixels), 0, 0,
                    skity::SamplingOptions{}, nullptr);
  return output.GetPixel(x, 0);
}

}  // namespace

TEST(ColorSpaceDecodeReview, ScalePixelsHonorsLinearDestination) {
  auto source = ReviewSolidPixmap(
      8, skity::kUnpremul_AlphaType,
      skity::ColorSetARGB(255, 64, 64, 64), skity::ColorSpace::SRGBLinear());
  auto dst = ReviewSolidPixmap(1, skity::kUnpremul_AlphaType, 0,
                              skity::ColorSpace::SRGBLinear());
  ASSERT_TRUE(skity::Image::MakeImage(source)->ScalePixels(
      dst, nullptr, skity::SamplingOptions{}));
  ASSERT_TRUE(skity::ColorSpaceEquals(dst->GetColorSpace(),
                                     skity::ColorSpace::SRGBLinear()));
  EXPECT_NEAR(dst->Addr8(0, 0)[0], 64, 1);
  // Resizing a solid image must preserve its displayed color, including
  // when the caller supplies a destination in the same source space.
  auto before = DrawReviewPixmap(source, skity::Color_BLACK);
  auto after = DrawReviewPixmap(dst, skity::Color_BLACK);
  std::printf("scale linear: stored=%u before_draw=%u after_draw=%u\n",
              dst->Addr8(0, 0)[0], ColorGetR(before), ColorGetR(after));
  EXPECT_NEAR(ColorGetR(after), ColorGetR(before), 1);
}

TEST(ColorSpaceDecodeReview, PremulP3RedMustNotOverflowWhiteBackground) {
  auto source = ReviewSolidPixmap(
      9, skity::kPremul_AlphaType, skity::ColorSetARGB(128, 128, 0, 0),
      skity::ColorSpace::DisplayP3());
  skity::Bitmap input(source);
  skity::BitmapSampler sampler(input, skity::SamplingOptions{},
                               skity::TileMode::kClamp,
                               skity::TileMode::kClamp);
  auto sample = sampler.GetColor({0.5f, 0.5f});
  std::printf("P3 premul sample RGBA=(%u,%u,%u,%u)\n", ColorGetR(sample),
              ColorGetG(sample), ColorGetB(sample), ColorGetA(sample));
  // The 8-bit software blender requires normalized premultiplied input.
  EXPECT_LE(ColorGetR(sample), ColorGetA(sample));
  for (uint32_t x : {0u, 8u}) {
    auto color = DrawReviewPixmap(source, skity::Color_WHITE, x);
    std::printf("white background pixel %u RGBA=(%u,%u,%u,%u)\n", x,
                ColorGetR(color), ColorGetG(color), ColorGetB(color),
                ColorGetA(color));
    EXPECT_EQ(ColorGetA(color), 255u) << "pixel " << x;
    EXPECT_GE(ColorGetR(color), 254u) << "pixel " << x;
    EXPECT_NEAR(ColorGetG(color), 127, 1) << "pixel " << x;
    EXPECT_NEAR(ColorGetB(color), 127, 1) << "pixel " << x;
  }
}

TEST(ColorSpaceDecodeReview, PremulAndUnpremulP3RepresentSameColor) {
  auto premul = ReviewSolidPixmap(
      1, skity::kPremul_AlphaType, skity::ColorSetARGB(128, 128, 0, 0),
      skity::ColorSpace::DisplayP3());
  auto unpremul = ReviewSolidPixmap(
      1, skity::kUnpremul_AlphaType, skity::ColorSetARGB(128, 255, 0, 0),
      skity::ColorSpace::DisplayP3());
  auto p = DrawReviewPixmap(premul, skity::Color_BLACK);
  auto u = DrawReviewPixmap(unpremul, skity::Color_BLACK);
  std::printf("black background: premul red=%u unpremul red=%u\n",
              ColorGetR(p), ColorGetR(u));
  EXPECT_NEAR(ColorGetR(p), ColorGetR(u), 1);
}

TEST(ColorSpaceDecodeReview, MeasureRepeatedConversionPreparation) {
  constexpr int kCount = 1024 * 1024;
  std::vector<skity::Color4f> input(kCount), singles(kCount), batch(kCount);
  for (int i = 0; i < kCount; ++i) {
    input[i] = {((i * 37) % 256) / 255.f, ((i * 73) % 256) / 255.f,
                ((i * 19) % 256) / 255.f, 1.f};
  }
  auto src = skity::ColorSpace::DisplayP3();
  std::shared_ptr<const skity::ColorSpace> dst;
  std::vector<double> single_ms, batch_ms;
  for (int repeat = 0; repeat < 6; ++repeat) {
    // Both paths produce identical colors. Only preparation reuse differs.
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kCount; ++i) {
      singles[i] = skity::ColorSpace::Convert(src, input[i], dst);
    }
    auto stop = std::chrono::steady_clock::now();
    batch = input;
    auto batch_start = std::chrono::steady_clock::now();
    skity::ColorSpace::ConvertColors(src, batch.data(), kCount, dst);
    auto batch_stop = std::chrono::steady_clock::now();
    if (repeat > 0) {
      single_ms.push_back(
          std::chrono::duration<double, std::milli>(stop - start).count());
      batch_ms.push_back(std::chrono::duration<double, std::milli>(
                             batch_stop - batch_start).count());
    }
  }
  for (int i = 0; i < kCount; ++i) {
    ASSERT_EQ(singles[i], batch[i]);
  }
  std::sort(single_ms.begin(), single_ms.end());
  std::sort(batch_ms.begin(), batch_ms.end());
  RecordProperty("per_color_ms", std::to_string(single_ms[2]));
  RecordProperty("prepared_once_ms", std::to_string(batch_ms[2]));
  RecordProperty("ratio", std::to_string(single_ms[2] / batch_ms[2]));
  std::printf("P3 to sRGB 1024x1024 median: per-color %.3f ms, prepared-once "
              "%.3f ms, ratio %.2f\n", single_ms[2], batch_ms[2],
              single_ms[2] / batch_ms[2]);
}

TEST(ColorSpaceDecodeReview, UnsupportedScaleDestinationIsRejected) {
  // Scaling between two different declared spaces is not implemented; the
  // call must fail outright instead of returning working-space bytes under
  // the destination's tag.
  auto source = ReviewSolidPixmap(
      8, skity::kUnpremul_AlphaType, skity::ColorSetARGB(255, 64, 64, 64),
      skity::ColorSpace::SRGBLinear());
  auto dst = ReviewSolidPixmap(1, skity::kUnpremul_AlphaType,
                               skity::ColorSetARGB(255, 7, 7, 7),
                               skity::ColorSpace::DisplayP3());
  EXPECT_FALSE(skity::Image::MakeImage(source)->ScalePixels(
      dst, nullptr, skity::SamplingOptions{}));
  // The destination was not partially written.
  EXPECT_EQ(dst->Addr8(0, 0)[0], 7);
  EXPECT_TRUE(
      skity::ColorSpaceEquals(dst->GetColorSpace(), skity::ColorSpace::DisplayP3()));
}

