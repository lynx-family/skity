// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <skity/codec/codec.hpp>
#include <skity/effect/shader.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/graphic/image.hpp>
#include <skity/io/pixmap.hpp>

#include "common/golden_test_check.hpp"
#include "common/golden_test_env.hpp"
#include "src/graphic/color_transform.hpp"

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

std::shared_ptr<const skity::ColorSpace> Gamma22TargetSpace() {
  const float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  return skity::ColorSpace::MakeRGB(tf, skity::ColorSpace::SRGB()->gamut_xyz);
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

// The same decoding images rendered onto a Display P3 target: the P3 image
// passes through byte exactly and the linear image encodes into P3, pinned
// by a baseline image (the visual artifact of target-space rendering).
TEST(ColorSpaceGolden, DisplayP3TargetDecodeBaseline) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  env->SetTargetColorSpace(skity::ColorSpace::DisplayP3());
  skity::testing::GoldenTestEnvConfig config;
  config.sample_count = 1;
  auto expected =
      std::filesystem::path(kGoldenTestImageDir) / "p3_target_decode.png";
  SCOPED_TRACE(expected.string());
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(64, 64, expected.c_str(),
                                                   config, DrawDecodingImages));
  env->SetTargetColorSpace(nullptr);
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

// Target-space semantics: on a Display P3 surface a P3-tagged image passes
// through byte exactly (no sRGB round trip, so the P3 red primary survives
// unclipped), while an sRGB-tagged image converts into its P3 representation.
// Halves are full height so the assertions are readback-orientation safe.
TEST(ColorSpaceGolden, P3TargetSpaceImagePassthrough) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  env->SetTargetColorSpace(skity::ColorSpace::DisplayP3());

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);

    auto p3_red = MakeSolidImage(32, 64, skity::Color4f{1.f, 0.f, 0.f, 1.f},
                                 skity::ColorSpace::DisplayP3());
    canvas->DrawImageRect(p3_red, skity::Rect::MakeWH(32.f, 64.f),
                          skity::Rect::MakeXYWH(0.f, 0.f, 32.f, 64.f),
                          skity::SamplingOptions{});

    auto srgb_red =
        MakeSolidImage(32, 64, skity::Color4f{1.f, 0.f, 0.f, 1.f}, nullptr);
    canvas->DrawImageRect(srgb_red, skity::Rect::MakeWH(32.f, 64.f),
                          skity::Rect::MakeXYWH(32.f, 0.f, 32.f, 64.f),
                          skity::SamplingOptions{});
  });
  env->SetTargetColorSpace(nullptr);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  // The P3 red primary (outside sRGB) must land byte exactly; a pipeline
  // that routed it through an sRGB working space would return it muted
  // (approximately 240/255, 49/255, 28/255 in P3 bytes).
  auto p3_result = skity::Color4fFromColor(bitmap.GetPixel(10, 32));
  EXPECT_EQ(p3_result.r, 1.f);
  EXPECT_EQ(p3_result.g, 0.f);
  EXPECT_EQ(p3_result.b, 0.f);
  EXPECT_EQ(p3_result.a, 1.f);

  auto expected =
      skity::ColorSpace::Convert(nullptr, skity::Color4f{1.f, 0.f, 0.f, 1.f},
                                 skity::ColorSpace::DisplayP3());
  auto srgb_result = skity::Color4fFromColor(bitmap.GetPixel(54, 32));
  EXPECT_NEAR(srgb_result.r, expected.r, 3.f / 255.f);
  EXPECT_NEAR(srgb_result.g, expected.g, 3.f / 255.f);
  EXPECT_NEAR(srgb_result.b, expected.b, 3.f / 255.f);
  EXPECT_NEAR(srgb_result.a, 1.f, 1.f / 255.f);
}

// A paint color recorded in the target space resolves without conversion;
// the whole-canvas fill is orientation safe.
TEST(ColorSpaceGolden, P3TargetSpacePaintColor) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  env->SetTargetColorSpace(skity::ColorSpace::DisplayP3());

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    skity::Paint paint;
    paint.SetStyle(skity::Paint::kFill_Style);
    paint.SetColor4f(skity::Color4f{0.f, 1.f, 0.f, 1.f},
                     skity::ColorSpace::DisplayP3());
    canvas->DrawRect(skity::Rect::MakeWH(64.f, 64.f), paint);
  });
  env->SetTargetColorSpace(nullptr);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto result = skity::Color4fFromColor(bitmap.GetPixel(32, 32));
  EXPECT_NEAR(result.r, 0.f, 1.f / 255.f);
  EXPECT_NEAR(result.g, 1.f, 1.f / 255.f);
  EXPECT_NEAR(result.b, 0.f, 1.f / 255.f);
  EXPECT_NEAR(result.a, 1.f, 1.f / 255.f);
}

// A paint color recorded in a DIFFERENT space than the target must actually
// convert at draw time on the GPU (the no-op case above cannot detect a
// missing resolution). sRGB green {0.2, 0.9, 0.2} rendered into a P3 target
// stores the P3 encoding of that sRGB color (~131, 224, 73), visibly
// different from the raw bytes (51, 229, 51).
TEST(ColorSpaceGolden, P3TargetSpaceResolvesSRGBPaintColor) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  env->SetTargetColorSpace(skity::ColorSpace::DisplayP3());

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);
    skity::Paint paint;
    paint.SetStyle(skity::Paint::kFill_Style);
    paint.SetColor4f(skity::Color4f{0.2f, 0.9f, 0.2f, 1.f}, nullptr);
    // Half width: a full-screen opaque SrcOver would be promoted to the
    // initial clear (which resolves separately) and never exercise the
    // rasterize path this case guards.
    canvas->DrawRect(skity::Rect::MakeWH(32.f, 64.f), paint);
  });
  env->SetTargetColorSpace(nullptr);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto expected =
      skity::ColorSpace::Convert(nullptr, skity::Color4f{0.2f, 0.9f, 0.2f, 1.f},
                                 skity::ColorSpace::DisplayP3());
  auto result = skity::Color4fFromColor(bitmap.GetPixel(16, 32));
  EXPECT_NEAR(result.r, expected.r, 3.f / 255.f);
  EXPECT_NEAR(result.g, expected.g, 3.f / 255.f);
  EXPECT_NEAR(result.b, expected.b, 3.f / 255.f);
  EXPECT_NEAR(result.a, 1.f, 1.f / 255.f);
  // Guard against a missing resolution: the raw bytes would keep r at ~0.2.
  EXPECT_GT(result.r, 0.4f);
}

// A linear sRGB target folds the encode stage away entirely (GPU flags:
// decode only), so an sRGB 0.5 gray stores its linear value.
TEST(ColorSpaceGolden, LinearTargetFoldsEncode) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  env->SetTargetColorSpace(skity::ColorSpace::SRGBLinear());

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);
    auto image =
        MakeSolidImage(64, 64, skity::Color4f{0.5f, 0.5f, 0.5f, 1.f}, nullptr);
    canvas->DrawImageRect(image, skity::Rect::MakeWH(64.f, 64.f),
                          skity::Rect::MakeWH(64.f, 64.f),
                          skity::SamplingOptions{});
  });
  env->SetTargetColorSpace(nullptr);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto result = skity::Color4fFromColor(bitmap.GetPixel(32, 32));
  EXPECT_NEAR(result.r, 0.21404114f, 3.f / 255.f);
  EXPECT_NEAR(result.g, 0.21404114f, 3.f / 255.f);
  EXPECT_NEAR(result.b, 0.21404114f, 3.f / 255.f);
  EXPECT_NEAR(result.a, 1.f, 1.f / 255.f);
}

// A gamma 2.2 target has no closed constant curve: the shader must take the
// parametric encode branch (cs_enc_tf uniforms).
TEST(ColorSpaceGolden, Gamma22TargetParametricEncode) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  auto target = Gamma22TargetSpace();
  env->SetTargetColorSpace(target);

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);
    auto image =
        MakeSolidImage(64, 64, skity::Color4f{0.5f, 0.5f, 0.5f, 1.f}, nullptr);
    canvas->DrawImageRect(image, skity::Rect::MakeWH(64.f, 64.f),
                          skity::Rect::MakeWH(64.f, 64.f),
                          skity::SamplingOptions{});
  });
  env->SetTargetColorSpace(nullptr);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto expected = skity::ColorSpace::Convert(
      nullptr, skity::Color4f{0.5f, 0.5f, 0.5f, 1.f}, target);
  auto result = skity::Color4fFromColor(bitmap.GetPixel(32, 32));
  EXPECT_NEAR(result.r, expected.r, 3.f / 255.f);
  EXPECT_NEAR(result.g, expected.g, 3.f / 255.f);
  EXPECT_NEAR(result.b, expected.b, 3.f / 255.f);
  EXPECT_NEAR(result.a, 1.f, 1.f / 255.f);
}

// saveLayer content renders into an offscreen texture tagged with the
// surface space; compositing back must pass the P3 red primary through byte
// exactly (no accidental sRGB reinterpretation of the layer texture).
TEST(ColorSpaceGolden, P3TargetSaveLayerComposite) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  env->SetTargetColorSpace(skity::ColorSpace::DisplayP3());

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);

    skity::Paint layer_paint;
    canvas->SaveLayer(skity::Rect::MakeWH(64.f, 64.f), layer_paint);

    auto p3_red = MakeSolidImage(32, 64, skity::Color4f{1.f, 0.f, 0.f, 1.f},
                                 skity::ColorSpace::DisplayP3());
    canvas->DrawImageRect(p3_red, skity::Rect::MakeWH(32.f, 64.f),
                          skity::Rect::MakeXYWH(0.f, 0.f, 32.f, 64.f),
                          skity::SamplingOptions{});
    canvas->Restore();
  });
  env->SetTargetColorSpace(nullptr);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto result = skity::Color4fFromColor(bitmap.GetPixel(10, 32));
  EXPECT_EQ(result.r, 1.f);
  EXPECT_EQ(result.g, 0.f);
  EXPECT_EQ(result.b, 0.f);
  EXPECT_EQ(result.a, 1.f);
}

// Gradient stops are recorded in sRGB and resolve into the target space; the
// clamped regions outside the gradient segment sample the resolved stop
// colors exactly, independent of any interpolation convention.
TEST(ColorSpaceGolden, P3TargetGradientStopsResolve) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  env->SetTargetColorSpace(skity::ColorSpace::DisplayP3());

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);

    const skity::Point pts[2] = {{16.f, 0.f, 0.f, 1.f}, {48.f, 0.f, 0.f, 1.f}};
    const skity::Vec4 stops[2] = {{1.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}};
    auto shader = skity::Shader::MakeLinear(pts, stops, nullptr, 2);

    skity::Paint paint;
    paint.SetShader(shader);
    canvas->DrawRect(skity::Rect::MakeWH(64.f, 64.f), paint);
  });
  env->SetTargetColorSpace(nullptr);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto red_p3 =
      skity::ColorSpace::Convert(nullptr, skity::Color4f{1.f, 0.f, 0.f, 1.f},
                                 skity::ColorSpace::DisplayP3());
  auto blue_p3 =
      skity::ColorSpace::Convert(nullptr, skity::Color4f{0.f, 0.f, 1.f, 1.f},
                                 skity::ColorSpace::DisplayP3());

  // Left of the segment start: the resolved red stop, byte exactly.
  auto left = skity::Color4fFromColor(bitmap.GetPixel(8, 32));
  EXPECT_NEAR(left.r, red_p3.r, 3.f / 255.f);
  EXPECT_NEAR(left.g, red_p3.g, 3.f / 255.f);
  EXPECT_NEAR(left.b, red_p3.b, 3.f / 255.f);
  EXPECT_NEAR(left.a, 1.f, 1.f / 255.f);

  // Right of the segment end: the resolved blue stop.
  auto right = skity::Color4fFromColor(bitmap.GetPixel(56, 32));
  EXPECT_NEAR(right.r, blue_p3.r, 3.f / 255.f);
  EXPECT_NEAR(right.g, blue_p3.g, 3.f / 255.f);
  EXPECT_NEAR(right.b, blue_p3.b, 3.f / 255.f);
  EXPECT_NEAR(right.a, 1.f, 1.f / 255.f);
}

// Blending stays in the target's encoded space: a P3 red fill at half alpha
// over white composites to (1, 0.5, 0.5) in P3 bytes. The P3 readback then
// converts back to sRGB through the CPU kernel and lands on the original
// sRGB red, closing the GPU-encode <-> CPU-decode loop.
TEST(ColorSpaceGolden, P3TargetAlphaCompositingAndReadbackRoundTrip) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  env->SetTargetColorSpace(skity::ColorSpace::DisplayP3());

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);
    skity::Paint paint;
    paint.SetStyle(skity::Paint::kFill_Style);
    paint.SetColor4f(skity::Color4f{1.f, 0.f, 0.f, 0.5f},
                     skity::ColorSpace::DisplayP3());
    canvas->DrawRect(skity::Rect::MakeWH(64.f, 64.f), paint);
  });
  env->SetTargetColorSpace(nullptr);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto blended = skity::Color4fFromColor(bitmap.GetPixel(32, 32));
  // The clear white also resolves into the P3 target. skity's sRGB gamut
  // constants are the ICC profile's (D50-adapted), so sRGB white lands
  // slightly off the P3 white point (~255, 252, 224) and the blended
  // channels fold that background in: out = dst * (1 - 0.5).
  auto bg =
      skity::ColorSpace::Convert(nullptr, skity::Color4f{1.f, 1.f, 1.f, 1.f},
                                 skity::ColorSpace::DisplayP3());
  auto q = [](float v) {
    return static_cast<float>(
               static_cast<uint8_t>(std::clamp(v * 255.f, 0.f, 255.f))) /
           255.f;
  };
  EXPECT_NEAR(blended.r, 1.f, 2.f / 255.f);  // Saturates.
  EXPECT_NEAR(blended.g, q(bg.g) * 0.5f, 2.f / 255.f);
  EXPECT_NEAR(blended.b, q(bg.b) * 0.5f, 2.f / 255.f);
  EXPECT_NEAR(blended.a, 1.f, 1.f / 255.f);

  // CPU round trip: the readback bytes are P3-encoded (the surface's
  // space); decode them through the CPU kernel with explicit steps and land
  // back on the sRGB view of the blended color.
  auto dst = std::make_shared<skity::Pixmap>(64, 64, skity::kPremul_AlphaType,
                                             skity::ColorType::kRGBA);
  auto steps = skity::ColorTransformSteps::Resolve(
      skity::ColorSpace::DisplayP3(), nullptr);
  ASSERT_TRUE(skity::TransformPixels(*pixels, dst.get(), steps));

  skity::Bitmap round_bitmap(dst, false);
  auto round = skity::Color4fFromColor(round_bitmap.GetPixel(32, 32));
  // Convert() keeps out-of-gamut values unclamped by contract; the byte
  // round trip clamps them at quantization, so clamp the expectation the
  // same way.
  auto blended_p3 = skity::Color4f{1.f, q(bg.g) * 0.5f, q(bg.b) * 0.5f, 1.f};
  auto round_expected = skity::ColorSpace::Convert(
      skity::ColorSpace::DisplayP3(), blended_p3, nullptr);
  auto clamp01 = [](float v) { return std::max(0.f, std::min(1.f, v)); };
  EXPECT_NEAR(round.r, clamp01(round_expected.r), 4.f / 255.f);
  EXPECT_NEAR(round.g, clamp01(round_expected.g), 4.f / 255.f);
  EXPECT_NEAR(round.b, clamp01(round_expected.b), 4.f / 255.f);
  EXPECT_NEAR(round.a, 1.f, 1.f / 255.f);
}
// Adobe RGB (1998): a gamma 2.2 space whose green primary sits well outside
// the sRGB gamut. An AdobeRGB-tagged image storing the CPU-converted
// representation of an sRGB color decodes back to that color on an sRGB
// target (parametric decode + gamut both active; the encode folds because
// the target curve is sRGB).
TEST(ColorSpaceGolden, AdobeRGBImageDecode) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);

    const skity::Color4f source{0.3f, 0.6f, 0.2f, 1.f};
    auto image =
        MakeSolidImage(64, 64,
                       skity::ColorSpace::Convert(
                           nullptr, source, skity::ColorSpace::AdobeRGB()),
                       skity::ColorSpace::AdobeRGB());
    canvas->DrawImageRect(image, skity::Rect::MakeWH(64.f, 64.f),
                          skity::Rect::MakeWH(64.f, 64.f),
                          skity::SamplingOptions{});
  });

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto result = skity::Color4fFromColor(bitmap.GetPixel(32, 32));
  EXPECT_NEAR(result.r, 0.3f, 3.f / 255.f);
  EXPECT_NEAR(result.g, 0.6f, 3.f / 255.f);
  EXPECT_NEAR(result.b, 0.2f, 3.f / 255.f);
  EXPECT_NEAR(result.a, 1.f, 1.f / 255.f);
}

// On an AdobeRGB target, an AdobeRGB-tagged image passes through byte
// exactly and an sRGB-recorded paint color resolves into its AdobeRGB
// encoding (a different gamut AND a different transfer curve than the
// previous golden targets).
TEST(ColorSpaceGolden, AdobeRGBTargetPassthroughAndPaint) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  env->SetTargetColorSpace(skity::ColorSpace::AdobeRGB());

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);

    auto green = MakeSolidImage(32, 64, skity::Color4f{0.f, 1.f, 0.f, 1.f},
                                skity::ColorSpace::AdobeRGB());
    canvas->DrawImageRect(green, skity::Rect::MakeWH(32.f, 64.f),
                          skity::Rect::MakeXYWH(0.f, 0.f, 32.f, 64.f),
                          skity::SamplingOptions{});

    // Half width keeps the paint fill on the rasterize path (a full-screen
    // opaque fill would be promoted to the initial clear).
    skity::Paint paint;
    paint.SetStyle(skity::Paint::kFill_Style);
    paint.SetColor4f(skity::Color4f{1.f, 0.f, 0.f, 1.f}, nullptr);
    canvas->DrawRect(skity::Rect::MakeXYWH(32.f, 0.f, 32.f, 64.f), paint);
  });
  env->SetTargetColorSpace(nullptr);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  // AdobeRGB green stores byte exactly (its primary is representable).
  auto green = skity::Color4fFromColor(bitmap.GetPixel(16, 32));
  EXPECT_EQ(green.g, 1.f);
  EXPECT_EQ(green.r, 0.f);
  EXPECT_EQ(green.b, 0.f);
  EXPECT_EQ(green.a, 1.f);

  // sRGB red resolves into its AdobeRGB encoding.
  auto expected =
      skity::ColorSpace::Convert(nullptr, skity::Color4f{1.f, 0.f, 0.f, 1.f},
                                 skity::ColorSpace::AdobeRGB());
  auto red = skity::Color4fFromColor(bitmap.GetPixel(48, 32));
  EXPECT_NEAR(red.r, expected.r, 3.f / 255.f);
  EXPECT_NEAR(red.g, expected.g, 3.f / 255.f);
  EXPECT_NEAR(red.b, expected.b, 3.f / 255.f);
  EXPECT_NEAR(red.a, 1.f, 1.f / 255.f);
}

// An sRGB-tagged image drawn onto an Adobe RGB target exercises the GPU
// texture decode with the gamut stage and the parametric (gamma 2.2)
// encode active at the same time - a combination none of the sRGB-curve
// targets (sRGB, Display P3) or same-gamut targets (gamma 2.2 over sRGB)
// can reach. sRGB green lands on its Adobe RGB encoding, computed here
// through the CPU conversion.
TEST(ColorSpaceGolden, AdobeRGBTargetResolvesSRGBImage) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  env->SetTargetColorSpace(skity::ColorSpace::AdobeRGB());

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);
    auto green =
        MakeSolidImage(64, 64, skity::Color4f{0.f, 1.f, 0.f, 1.f}, nullptr);
    canvas->DrawImageRect(green, skity::Rect::MakeWH(64.f, 64.f),
                          skity::Rect::MakeWH(64.f, 64.f),
                          skity::SamplingOptions{});
  });
  env->SetTargetColorSpace(nullptr);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto result = skity::Color4fFromColor(bitmap.GetPixel(32, 32));
  auto expected =
      skity::ColorSpace::Convert(nullptr, skity::Color4f{0.f, 1.f, 0.f, 1.f},
                                 skity::ColorSpace::AdobeRGB());
  auto q = [](float v) {
    return static_cast<float>(
               static_cast<uint8_t>(std::clamp(v * 255.f, 0.f, 255.f))) /
           255.f;
  };
  EXPECT_NEAR(result.r, q(expected.r), 2.f / 255.f);
  EXPECT_NEAR(result.g, q(expected.g), 2.f / 255.f);
  EXPECT_NEAR(result.b, q(expected.b), 2.f / 255.f);
  EXPECT_NEAR(result.a, 1.f, 1.f / 255.f);
}
