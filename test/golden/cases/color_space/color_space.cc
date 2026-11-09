// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <skity/effect/shader.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/io/pixmap.hpp>

#include "common/golden_test_check.hpp"
#include "common/golden_test_env.hpp"
#include "src/graphic/color_xform.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

namespace {

// Builds a w x h unpremul RGBA image whose every pixel is `color`, tagged
// with `color_space`.
std::shared_ptr<skity::Image> MakeSolidImage(uint32_t w, uint32_t h,
                                             skity::Color4f color,
                                             skity::ColorSpace color_space) {
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
  auto p3_image = MakeSolidImage(32, 64,
                                 skity::ColorSpace::SRGB().Convert(
                                     p3_source, skity::ColorSpace::DisplayP3()),
                                 skity::ColorSpace::DisplayP3());
  canvas->DrawImageRect(p3_image, skity::Rect::MakeWH(32.f, 64.f),
                        skity::Rect::MakeXYWH(0.f, 0.f, 32.f, 64.f),
                        skity::SamplingOptions{});

  // Right half: linear sRGB tagged image storing the linear representation of
  // another sRGB color.
  const skity::Color4f linear_source{0.25f, 0.5f, 0.75f, 1.f};
  auto linear_image =
      MakeSolidImage(32, 64,
                     skity::ColorSpace::SRGB().Convert(
                         linear_source, skity::ColorSpace::SRGBLinear()),
                     skity::ColorSpace::SRGBLinear());
  canvas->DrawImageRect(linear_image, skity::Rect::MakeWH(32.f, 64.f),
                        skity::Rect::MakeXYWH(32.f, 0.f, 32.f, 64.f),
                        skity::SamplingOptions{});
}

// Pure gamma 2.2 transfer over the sRGB gamut; only the curve differs from
// sRGB, so conversions exercise the parametric encode path.
skity::ColorSpace Gamma22TargetSpace() {
  const float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  return *skity::ColorSpace::MakeRGB(tf, skity::ColorSpace{}.gamut_xyz);
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

    auto srgb_red = MakeSolidImage(32, 64, skity::Color4f{1.f, 0.f, 0.f, 1.f},
                                   skity::ColorSpace{});
    canvas->DrawImageRect(srgb_red, skity::Rect::MakeWH(32.f, 64.f),
                          skity::Rect::MakeXYWH(32.f, 0.f, 32.f, 64.f),
                          skity::SamplingOptions{});
  });
  env->SetTargetColorSpace(skity::ColorSpace{});

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

  auto expected = skity::ColorSpace{}.Convert(
      skity::Color4f{1.f, 0.f, 0.f, 1.f}, skity::ColorSpace::DisplayP3());
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
  env->SetTargetColorSpace(skity::ColorSpace{});

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
    paint.SetColor4f(skity::Color4f{0.2f, 0.9f, 0.2f, 1.f},
                     skity::ColorSpace{});
    // Half width: a full-screen opaque SrcOver would be promoted to the
    // initial clear (which resolves separately) and never exercise the
    // rasterize path this case guards.
    canvas->DrawRect(skity::Rect::MakeWH(32.f, 64.f), paint);
  });
  env->SetTargetColorSpace(skity::ColorSpace{});

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto expected = skity::ColorSpace{}.Convert(
      skity::Color4f{0.2f, 0.9f, 0.2f, 1.f}, skity::ColorSpace::DisplayP3());
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
    auto image = MakeSolidImage(64, 64, skity::Color4f{0.5f, 0.5f, 0.5f, 1.f},
                                skity::ColorSpace{});
    canvas->DrawImageRect(image, skity::Rect::MakeWH(64.f, 64.f),
                          skity::Rect::MakeWH(64.f, 64.f),
                          skity::SamplingOptions{});
  });
  env->SetTargetColorSpace(skity::ColorSpace{});

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
    auto image = MakeSolidImage(64, 64, skity::Color4f{0.5f, 0.5f, 0.5f, 1.f},
                                skity::ColorSpace{});
    canvas->DrawImageRect(image, skity::Rect::MakeWH(64.f, 64.f),
                          skity::Rect::MakeWH(64.f, 64.f),
                          skity::SamplingOptions{});
  });
  env->SetTargetColorSpace(skity::ColorSpace{});

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto expected = skity::ColorSpace{}.Convert(
      skity::Color4f{0.5f, 0.5f, 0.5f, 1.f}, target);
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
  env->SetTargetColorSpace(skity::ColorSpace{});

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
  env->SetTargetColorSpace(skity::ColorSpace{});

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto red_p3 = skity::ColorSpace{}.Convert(skity::Color4f{1.f, 0.f, 0.f, 1.f},
                                            skity::ColorSpace::DisplayP3());
  auto blue_p3 = skity::ColorSpace{}.Convert(skity::Color4f{0.f, 0.f, 1.f, 1.f},
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
  env->SetTargetColorSpace(skity::ColorSpace{});

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto blended = skity::Color4fFromColor(bitmap.GetPixel(32, 32));
  // The clear white also resolves into the P3 target. skity's sRGB gamut
  // constants are the ICC profile's (D50-adapted), so sRGB white lands
  // slightly off the P3 white point (~255, 252, 224) and the blended
  // channels fold that background in: out = dst * (1 - 0.5).
  auto bg = skity::ColorSpace{}.Convert(skity::Color4f{1.f, 1.f, 1.f, 1.f},
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
  auto steps = skity::ColorXformSteps::Resolve(skity::ColorSpace::DisplayP3(),
                                               skity::ColorSpace{});
  ASSERT_TRUE(skity::TransformPixels(*pixels, dst.get(), steps));

  skity::Bitmap round_bitmap(dst, false);
  auto round = skity::Color4fFromColor(round_bitmap.GetPixel(32, 32));
  // Convert() keeps out-of-gamut values unclamped by contract; the byte
  // round trip clamps them at quantization, so clamp the expectation the
  // same way.
  auto blended_p3 = skity::Color4f{1.f, q(bg.g) * 0.5f, q(bg.b) * 0.5f, 1.f};
  auto round_expected =
      skity::ColorSpace::DisplayP3().Convert(blended_p3, skity::ColorSpace{});
  auto clamp01 = [](float v) { return std::max(0.f, std::min(1.f, v)); };
  EXPECT_NEAR(round.r, clamp01(round_expected.r), 4.f / 255.f);
  EXPECT_NEAR(round.g, clamp01(round_expected.g), 4.f / 255.f);
  EXPECT_NEAR(round.b, clamp01(round_expected.b), 4.f / 255.f);
  EXPECT_NEAR(round.a, 1.f, 1.f / 255.f);
}

// Gradient stops declared in a non-sRGB space resolve into the target: a
// gradient whose stops are the P3 encodings of sRGB red/blue, declared in
// P3, lands back on the original sRGB colors on an sRGB target (sampled in
// the clamped regions outside the gradient segment).
TEST(ColorSpaceGolden, P3DeclaredStopsResolveIntoSRGBTarget) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);

  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_WHITE);

    const skity::Color4f srgb_red{1.f, 0.f, 0.f, 1.f};
    const skity::Color4f srgb_blue{0.f, 0.f, 1.f, 1.f};
    const skity::Vec4 stops[2] = {
        skity::ColorSpace::SRGB().Convert(srgb_red,
                                          skity::ColorSpace::DisplayP3()),
        skity::ColorSpace::SRGB().Convert(srgb_blue,
                                          skity::ColorSpace::DisplayP3())};
    const skity::Point pts[2] = {{16.f, 0.f, 0.f, 1.f}, {48.f, 0.f, 0.f, 1.f}};

    auto shader = skity::Shader::MakeLinear(pts, stops, nullptr, 2);
    shader->SetColorSpace(skity::ColorSpace::DisplayP3());

    skity::Paint paint;
    paint.SetShader(shader);
    canvas->DrawRect(skity::Rect::MakeWH(64.f, 64.f), paint);
  });

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);

  skity::Bitmap bitmap(pixels);
  auto left = skity::Color4fFromColor(bitmap.GetPixel(8, 32));
  EXPECT_NEAR(left.r, 1.f, 3.f / 255.f);
  EXPECT_NEAR(left.g, 0.f, 3.f / 255.f);
  EXPECT_NEAR(left.b, 0.f, 3.f / 255.f);
  EXPECT_NEAR(left.a, 1.f, 1.f / 255.f);

  auto right = skity::Color4fFromColor(bitmap.GetPixel(56, 32));
  EXPECT_NEAR(right.r, 0.f, 3.f / 255.f);
  EXPECT_NEAR(right.g, 0.f, 3.f / 255.f);
  EXPECT_NEAR(right.b, 1.f, 3.f / 255.f);
  EXPECT_NEAR(right.a, 1.f, 1.f / 255.f);
}
