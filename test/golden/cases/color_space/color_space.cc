// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/graphic/image.hpp>
#include <skity/io/pixmap.hpp>

#include "common/golden_test_check.hpp"
#include "common/golden_test_env.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

namespace {

// Builds a w x h unpremul RGBA image whose every pixel is `color`, tagged
// with `color_space`.
std::shared_ptr<skity::Image> MakeSolidImage(
    uint32_t w, uint32_t h, skity::Color4f color,
    std::shared_ptr<const skity::ColorSpace> color_space) {
  auto pixmap = std::make_shared<skity::Pixmap>(w, h);
  skity::Bitmap bitmap(pixmap, false);
  auto stored = skity::Color4fToColor(color);
  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      bitmap.SetPixel(x, y, stored);
    }
  }
  pixmap->SetColorInfo(skity::kUnpremul_AlphaType, skity::ColorType::kRGBA,
                       color_space);
  return skity::Image::MakeImage(pixmap);
}

void DrawDecodingImages(skity::Canvas* canvas) {
  canvas->Clear(skity::Color_WHITE);

  // Left half: Display P3 tagged image storing the CPU-converted P3
  // representation of an sRGB color.
  const skity::Color4f p3_source{0.9f, 0.5f, 0.1f, 1.f};
  auto p3_image = MakeSolidImage(
      32, 64,
      skity::ColorSpace::Convert(skity::ColorSpace::SRGB(), p3_source,
                                 skity::ColorSpace::DisplayP3()),
      skity::ColorSpace::DisplayP3());
  canvas->DrawImageRect(p3_image, skity::Rect::MakeWH(32.f, 64.f),
                        skity::Rect::MakeXYWH(0.f, 0.f, 32.f, 64.f),
                        skity::SamplingOptions{});

  // Right half: linear sRGB tagged image storing the linear representation of
  // another sRGB color.
  const skity::Color4f linear_source{0.25f, 0.5f, 0.75f, 1.f};
  auto linear_image = MakeSolidImage(
      32, 64,
      skity::ColorSpace::Convert(skity::ColorSpace::SRGB(), linear_source,
                                 skity::ColorSpace::SRGBLinear()),
      skity::ColorSpace::SRGBLinear());
  canvas->DrawImageRect(linear_image, skity::Rect::MakeWH(32.f, 64.f),
                        skity::Rect::MakeXYWH(32.f, 0.f, 32.f, 64.f),
                        skity::SamplingOptions{});
}

}  // namespace

// The GPU texture decode must land back on the original sRGB colors: the P3
// image stores CPU-converted P3 values and the linear image stores linear
// values; both decode in the shader to the working space. The solid fills
// are vertically symmetric, so the assertion is readback-orientation safe.
TEST(ColorSpaceGolden, TextureDecodeMatchesCPUConversion) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  auto texture = env->RenderToTexture(64, 64, DrawDecodingImages);
  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto p3_result = skity::Color4fFromColor(bitmap.GetPixel(10, 32));
  EXPECT_NEAR(p3_result.r, 0.9f, 3.f / 255.f);
  EXPECT_NEAR(p3_result.g, 0.5f, 3.f / 255.f);
  EXPECT_NEAR(p3_result.b, 0.1f, 3.f / 255.f);
  EXPECT_NEAR(p3_result.a, 1.f, 1.f / 255.f);

  auto linear_result = skity::Color4fFromColor(bitmap.GetPixel(54, 32));
  EXPECT_NEAR(linear_result.r, 0.25f, 3.f / 255.f);
  EXPECT_NEAR(linear_result.g, 0.5f, 3.f / 255.f);
  EXPECT_NEAR(linear_result.b, 0.75f, 3.f / 255.f);
  EXPECT_NEAR(linear_result.a, 1.f, 1.f / 255.f);
}

TEST(ColorSpaceGolden, DisplayP3AndLinearSRGBDecode) {
  skity::testing::GoldenTestEnvConfig config;
  config.sample_count = 1;
  auto expected =
      std::filesystem::path(kGoldenTestImageDir) / "p3_linear_decode.png";
  SCOPED_TRACE(expected.string());
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(64, 64, expected.c_str(),
                                                   config, DrawDecodingImages));
}

// GPU ScalePixels must go through the same color space decode as a draw:
// the source texture holds linear-encoded bytes, and scaling them to 1x1
// without the source tag would hand back the raw 64 instead of the
// sRGB-encoded ~137.
TEST(ColorSpaceGolden, TextureScalePreservesSourceColorSpace) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  auto* context = env->GetGPUContext();

  auto pixmap = std::make_shared<skity::Pixmap>(
      8, 8, skity::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kUnpremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::SRGBLinear()));
  for (uint32_t y = 0; y < 8; y++) {
    for (uint32_t x = 0; x < 8; x++) {
      auto addr = pixmap->WritableAddr8(x, y);
      addr[0] = addr[1] = addr[2] = 64;
      addr[3] = 255;
    }
  }
  auto image = skity::Image::MakeImage(pixmap, context);
  ASSERT_NE(image, nullptr);

  auto dst = std::make_shared<skity::Pixmap>(1, 1, skity::kUnpremul_AlphaType,
                                             skity::ColorType::kRGBA);
  ASSERT_TRUE(image->ScalePixels(dst, context, skity::SamplingOptions{}));

  auto pixel = static_cast<const uint8_t*>(dst->Addr());
  EXPECT_EQ(pixel[3], 255);
  EXPECT_NEAR(pixel[0], 137, 2);
  EXPECT_NEAR(pixel[1], 137, 2);
  EXPECT_NEAR(pixel[2], 137, 2);
}
