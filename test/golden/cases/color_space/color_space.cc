// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <skity/codec/codec.hpp>
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

// Premultiplied textures decode through the unpremultiplied domain and
// re-premultiply on the way out: a semi-transparent P3 pixel must land on
// the same value the CPU sampler computes, and a fully transparent pixel
// must stay exactly zero (no black-point leak through the re-premultiply).
TEST(ColorSpaceGolden, PremulTextureDecodeRepremultiplies) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);

  // sRGB (0.9, 0.5, 0.1) pre-converted into P3, stored premultiplied with
  // alpha = 128/255 in a 64x64 tagged texture: left half carries the color,
  // right half is fully transparent.
  const skity::Color4f source{0.9f, 0.5f, 0.1f, 1.f};
  auto p3 = skity::ColorSpace::Convert(skity::ColorSpace::SRGB(), source,
                                       skity::ColorSpace::DisplayP3());
  auto pixmap = std::make_shared<skity::Pixmap>(
      64, 64, skity::kPremul_AlphaType, skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kPremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::DisplayP3()));
  const float alpha = 128.f / 255.f;
  for (uint32_t y = 0; y < 64; y++) {
    for (uint32_t x = 0; x < 64; x++) {
      auto addr = pixmap->WritableAddr8(x, y);
      if (x < 32) {
        addr[0] = static_cast<uint8_t>(p3.r * 128.f + 0.5f);
        addr[1] = static_cast<uint8_t>(p3.g * 128.f + 0.5f);
        addr[2] = static_cast<uint8_t>(p3.b * 128.f + 0.5f);
        addr[3] = 128;
      } else {
        addr[0] = addr[1] = addr[2] = addr[3] = 0;
      }
    }
  }

  auto image = skity::Image::MakeImage(pixmap, env->GetGPUContext());
  ASSERT_NE(image, nullptr);

  auto draw = [&](skity::Canvas* canvas) {
    canvas->Clear(skity::ColorSetARGB(0, 0, 0, 0));
    canvas->DrawImageRect(image, skity::Rect::MakeWH(64.f, 64.f),
                          skity::Rect::MakeWH(64.f, 64.f),
                          skity::SamplingOptions{});
  };
  auto texture = env->RenderToTexture(64, 64, draw);
  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  skity::Bitmap bitmap(pixels);

  // Expected: same math as the CPU sampler on the stored bytes.
  auto stored = skity::Color4fFromColor(
      skity::ColorSetARGB(128, static_cast<uint8_t>(p3.r * 128.f + 0.5f),
                          static_cast<uint8_t>(p3.g * 128.f + 0.5f),
                          static_cast<uint8_t>(p3.b * 128.f + 0.5f)));
  auto unpremul = skity::ColorSpace::Convert(
      skity::ColorSpace::DisplayP3(),
      skity::Color4f{stored.r / alpha, stored.g / alpha, stored.b / alpha,
                     alpha},
      nullptr);
  skity::Color4f expected{unpremul.r * alpha, unpremul.g * alpha,
                          unpremul.b * alpha, alpha};

  auto result = skity::Color4fFromColor(bitmap.GetPixel(10, 32));
  EXPECT_NEAR(result.r, expected.r, 3.f / 255.f);
  EXPECT_NEAR(result.g, expected.g, 3.f / 255.f);
  EXPECT_NEAR(result.b, expected.b, 3.f / 255.f);
  EXPECT_NEAR(result.a, alpha, 1.f / 255.f);

  // The transparent half stays exactly zero.
  auto transparent = bitmap.GetPixel(54, 32);
  EXPECT_EQ(ColorGetA(transparent), 0);
  EXPECT_LE(ColorGetR(transparent), 1);
  EXPECT_LE(ColorGetG(transparent), 1);
  EXPECT_LE(ColorGetB(transparent), 1);
}

// Full chain through the codec on the GPU: a PNG carrying gAMA/cHRM chunks
// decodes to a tagged pixmap, the tag rides into the texture, and the draw
// lands back on the original sRGB color. The expected value is computed
// from the decoded bytes and tag (not the encode-side values), so the
// assertion does not depend on the chunk quantization.
TEST(ColorSpaceGolden, CodecTaggedPNGDecodesOnGPU) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);

  // sRGB (0.9, 0.5, 0.1) stored as a gamma-2.2 P3 solid image.
  const skity::Color4f source{0.9f, 0.5f, 0.1f, 1.f};
  float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  auto space =
      skity::ColorSpace::MakeRGB(tf, skity::ColorSpace::DisplayP3()->gamut_xyz);
  ASSERT_NE(space, nullptr);
  auto stored4f =
      skity::ColorSpace::Convert(skity::ColorSpace::SRGB(), source, space);
  auto stored = skity::Color4fToColor(stored4f);
  skity::Pixmap pixmap(64, 64, skity::kUnpremul_AlphaType,
                       skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap.SetColorInfo(skity::kUnpremul_AlphaType,
                                  skity::ColorType::kRGBA, space));
  for (uint32_t y = 0; y < 64; y++) {
    for (uint32_t x = 0; x < 64; x++) {
      auto addr = pixmap.WritableAddr8(x, y);
      addr[0] = ColorGetR(stored);
      addr[1] = ColorGetG(stored);
      addr[2] = ColorGetB(stored);
      addr[3] = 255;
    }
  }

  // Chunk round trip: the encoder writes gAMA/cHRM, the decoder tags the
  // decoded pixmap with the parametric space it read back.
  auto codec = skity::Codec::MakePngCodec();
  auto file = codec->Encode(&pixmap);
  ASSERT_NE(file, nullptr);
  codec->SetData(file);
  auto decoded = codec->Decode();
  ASSERT_NE(decoded, nullptr);
  ASSERT_NE(decoded->GetColorSpace(), nullptr);

  auto image = skity::Image::MakeImage(decoded, env->GetGPUContext());
  ASSERT_NE(image, nullptr);
  auto draw = [&](skity::Canvas* canvas) {
    canvas->Clear(skity::ColorSetARGB(0, 0, 0, 0));
    canvas->DrawImageRect(image, skity::Rect::MakeWH(64.f, 64.f),
                          skity::Rect::MakeWH(64.f, 64.f),
                          skity::SamplingOptions{});
  };
  auto texture = env->RenderToTexture(64, 64, draw);
  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  skity::Bitmap bitmap(pixels);

  // Expected: decode the actual stored bytes through the actual tag.
  auto decoded_bytes = static_cast<const uint8_t*>(decoded->Addr());
  auto decoded_pixel =
      skity::Color4f{decoded_bytes[0] / 255.f, decoded_bytes[1] / 255.f,
                     decoded_bytes[2] / 255.f, decoded_bytes[3] / 255.f};
  auto expected = skity::ColorSpace::Convert(decoded->GetColorSpace(),
                                             decoded_pixel, nullptr);
  auto result = skity::Color4fFromColor(bitmap.GetPixel(32, 32));
  EXPECT_NEAR(result.r, expected.r, 3.f / 255.f);
  EXPECT_NEAR(result.g, expected.g, 3.f / 255.f);
  EXPECT_NEAR(result.b, expected.b, 3.f / 255.f);
  EXPECT_NEAR(result.a, 1.f, 1.f / 255.f);

  // And the whole chain preserves the original color within the chunk
  // quantization.
  EXPECT_NEAR(result.r, source.r, 4.f / 255.f);
  EXPECT_NEAR(result.g, source.g, 4.f / 255.f);
  EXPECT_NEAR(result.b, source.b, 4.f / 255.f);
}

TEST(ColorSpaceDecodeReviewGPU, ScalePixelsHonorsLinearDestination) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  auto* context = env->GetGPUContext();
  auto pixmap = std::make_shared<skity::Pixmap>(
      8, 8, skity::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kUnpremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::SRGBLinear()));
  for (uint32_t y = 0; y < 8; ++y) {
    for (uint32_t x = 0; x < 8; ++x) {
      auto addr = pixmap->WritableAddr8(x, y);
      addr[0] = addr[1] = addr[2] = 64;
      addr[3] = 255;
    }
  }
  auto image = skity::Image::MakeImage(pixmap, context);
  auto dst = std::make_shared<skity::Pixmap>(1, 1, skity::kUnpremul_AlphaType,
                                             skity::ColorType::kRGBA);
  ASSERT_TRUE(dst->SetColorInfo(skity::kUnpremul_AlphaType,
                                skity::ColorType::kRGBA,
                                skity::ColorSpace::SRGBLinear()));
  ASSERT_TRUE(image->ScalePixels(dst, context, skity::SamplingOptions{}));
  ASSERT_TRUE(skity::ColorSpaceEquals(dst->GetColorSpace(),
                                      skity::ColorSpace::SRGBLinear()));
  std::printf("GPU ScalePixels linear destination stored R=%u\n",
              dst->Addr8(0, 0)[0]);
  EXPECT_NEAR(dst->Addr8(0, 0)[0], 64, 1);
}

// Out-of-gamut premultiplied colors clamp in the unpremultiplied domain on
// the GPU too (pure P3 red exceeds the sRGB working gamut): the readback
// must match the CPU sampler on the same input instead of carrying the
// extended range into the blend.
TEST(ColorSpaceGolden, PremulOutOfGamutClampsLikeCPU) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);

  // Premultiplied P3 red (unpremultiplied (255, 0, 0)) at half alpha.
  auto pixmap = std::make_shared<skity::Pixmap>(
      64, 64, skity::kPremul_AlphaType, skity::ColorType::kRGBA);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kPremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::DisplayP3()));
  for (uint32_t y = 0; y < 64; y++) {
    for (uint32_t x = 0; x < 64; x++) {
      auto addr = pixmap->WritableAddr8(x, y);
      addr[0] = 128;
      addr[1] = addr[2] = 0;
      addr[3] = 128;
    }
  }

  // Expected, computed the way the CPU sampler does it: convert the
  // unpremultiplied P3 red, clamp in the unpremultiplied domain, scale
  // back by alpha. R must not exceed the alpha.
  auto converted =
      skity::ColorSpace::Convert(skity::ColorSpace::DisplayP3(),
                                 skity::Color4f{1.f, 0.f, 0.f, 1.f}, nullptr);
  float alpha = 128.f / 255.f;
  float expected_rgb[3] = {std::clamp(converted.r, 0.f, 1.f),
                           std::clamp(converted.g, 0.f, 1.f),
                           std::clamp(converted.b, 0.f, 1.f)};
  auto expected = skity::ColorSetARGB(
      128, static_cast<uint8_t>(expected_rgb[0] * 128.f + 0.5f),
      static_cast<uint8_t>(expected_rgb[1] * 128.f + 0.5f),
      static_cast<uint8_t>(expected_rgb[2] * 128.f + 0.5f));
  EXPECT_LE(ColorGetR(expected), ColorGetA(expected));

  auto image = skity::Image::MakeImage(pixmap, env->GetGPUContext());
  ASSERT_NE(image, nullptr);
  auto draw = [&](skity::Canvas* canvas) {
    canvas->Clear(skity::ColorSetARGB(0, 0, 0, 0));
    canvas->DrawImageRect(image, skity::Rect::MakeWH(64.f, 64.f),
                          skity::Rect::MakeWH(64.f, 64.f),
                          skity::SamplingOptions{});
  };
  auto texture = env->RenderToTexture(64, 64, draw);
  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  skity::Bitmap bitmap(pixels);
  auto result = bitmap.GetPixel(32, 32);
  EXPECT_EQ(ColorGetR(result), ColorGetR(expected));
  EXPECT_EQ(ColorGetG(result), ColorGetG(expected));
  EXPECT_EQ(ColorGetB(result), ColorGetB(expected));
  EXPECT_EQ(ColorGetA(result), ColorGetA(expected));
}
