// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <skity/c/skity_picture.h>
#include <skity/c/skity_stream.h>
#include <unistd.h>

#include <cstdio>
#include <memory>
#include <skity/effect/color_filter.hpp>
#include <skity/gpu/gpu_render_target.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/graphic/image.hpp>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_env.hpp"
#include "handle.hpp"

namespace {

class ScopedTarget {
 public:
  ScopedTarget(skity::testing::GoldenTestEnv* env,
               std::shared_ptr<const skity::ColorSpace> space)
      : env_(env),
        space_(env->GetTargetColorSpace()),
        samples_(env->GetSampleCount()),
        contour_aa_(env->GetGPUContext()->IsEnableContourAA()) {
    env_->SetTargetColorSpace(std::move(space));
    env_->SetSampleCount(1);
  }
  ~ScopedTarget() {
    env_->SetTargetColorSpace(space_);
    env_->SetSampleCount(samples_);
    env_->GetGPUContext()->SetEnableContourAA(contour_aa_);
  }

 private:
  skity::testing::GoldenTestEnv* env_;
  std::shared_ptr<const skity::ColorSpace> space_;
  uint32_t samples_;
  bool contour_aa_;
};

void ExpectRGBNear(skity::Color actual, skity::Color expected, int tolerance) {
  EXPECT_NEAR(ColorGetR(actual), ColorGetR(expected), tolerance);
  EXPECT_NEAR(ColorGetG(actual), ColorGetG(expected), tolerance);
  EXPECT_NEAR(ColorGetB(actual), ColorGetB(expected), tolerance);
}

}  // namespace

TEST(ColorSpaceCoreReviewGPU, SnapshotRetainsP3Space) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  ScopedTarget scope(env, skity::ColorSpace::DisplayP3());
  auto* context = env->GetGPUContext();
  skity::GPURenderTargetDescriptor desc;
  desc.width = desc.height = 64;
  desc.sample_count = 1;
  desc.render_options.color_space = skity::ColorSpace::DisplayP3();
  auto target = context->CreateRenderTarget(desc);
  ASSERT_NE(target, nullptr);
  skity::Paint paint;
  paint.SetColor4f({1.f, 0.f, 0.f, 1.f}, skity::ColorSpace::DisplayP3());
  target->GetCanvas()->DrawRect(skity::Rect::MakeWH(64, 64), paint);
  auto snapshot = context->MakeSnapshot(std::move(target));
  ASSERT_NE(snapshot, nullptr);
  EXPECT_TRUE(skity::ColorSpaceEquals(snapshot->GetColorSpace(),
                                      skity::ColorSpace::DisplayP3()));
  auto pixels = snapshot->ReadPixels(context);
  ASSERT_NE(pixels, nullptr);
  EXPECT_TRUE(skity::ColorSpaceEquals(pixels->GetColorSpace(),
                                      skity::ColorSpace::DisplayP3()));
  skity::Bitmap source(pixels);
  EXPECT_EQ(source.GetPixel(32, 32), skity::Color_RED);
  auto replay = env->RenderToTexture(64, 64, [&](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_TRANSPARENT);
    canvas->DrawImage(snapshot, 0, 0);
  });
  ASSERT_NE(replay, nullptr);
  auto replay_pixels = replay->ReadPixels();
  ASSERT_NE(replay_pixels, nullptr);
  skity::Bitmap actual(replay_pixels);
  ExpectRGBNear(actual.GetPixel(32, 32), skity::Color_RED, 1);
}

TEST(ColorSpaceCoreReviewGPU, AAStrokePreservesRecordedColorSpace) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  ScopedTarget scope(env, nullptr);
  env->GetGPUContext()->SetEnableContourAA(true);
  for (bool aa : {false, true}) {
    SCOPED_TRACE(aa);
    auto texture = env->RenderToTexture(64, 64, [&](skity::Canvas* canvas) {
      canvas->Clear(skity::Color_TRANSPARENT);
      skity::Paint paint;
      paint.SetStyle(skity::Paint::kStroke_Style);
      paint.SetStrokeWidth(8.f);
      paint.SetAntiAlias(aa);
      paint.SetColor4f({0.25f, 0.25f, 0.25f, 1.f},
                       skity::ColorSpace::SRGBLinear());
      skity::Path path;
      path.MoveTo(8, 32).LineTo(56, 32).LineTo(56, 56);
      canvas->DrawPath(path, paint);
    });
    ASSERT_NE(texture, nullptr);
    auto pixels = texture->ReadPixels();
    ASSERT_NE(pixels, nullptr);
    skity::Bitmap bitmap(pixels);
    auto color = bitmap.GetPixel(24, 32);
    EXPECT_NEAR(ColorGetR(color), 137, 2);
    EXPECT_EQ(ColorGetA(color), 255u);
  }
}

TEST(ColorSpaceCoreReviewGPU, BlendFilterColorUsesTargetSpace) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  ScopedTarget scope(env, skity::ColorSpace::SRGBLinear());
  auto texture = env->RenderToTexture(64, 64, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_TRANSPARENT);
    skity::Paint paint;
    auto gray = skity::ColorSetARGB(255, 128, 128, 128);
    paint.SetColor(gray);
    canvas->DrawRect(skity::Rect::MakeWH(32, 64), paint);
    paint.SetColor(skity::Color_WHITE);
    paint.SetColorFilter(
        skity::ColorFilters::Blend(gray, skity::BlendMode::kSrc));
    canvas->DrawRect(skity::Rect::MakeXYWH(32, 0, 32, 64), paint);
  });
  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  skity::Bitmap bitmap(pixels);
  auto direct = bitmap.GetPixel(16, 32);
  auto filtered = bitmap.GetPixel(48, 32);
  EXPECT_NEAR(ColorGetR(direct), 55, 1);
  ExpectRGBNear(filtered, direct, 1);
}

TEST(ColorSpaceCoreReviewGPU, PictureSerializationPreservesPaintColor) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(8, 8));
  skity::Paint paint;
  paint.SetColor4f({0.25f, 0.25f, 0.25f, 1.f}, skity::ColorSpace::SRGBLinear());
  recorder.GetRecordingCanvas()->DrawRect(skity::Rect::MakeWH(8, 8), paint);
  std::shared_ptr<skity::DisplayList> dl(recorder.FinishRecording());
  ASSERT_NE(dl, nullptr);
  skity::Bitmap direct(8, 8, skity::kPremul_AlphaType);
  auto direct_canvas = skity::Canvas::MakeSoftwareCanvas(&direct);
  dl->Draw(direct_canvas.get());
  auto list_handle = skity::capi::alloc_handle<skity_display_list_s>(
      SKITY_OBJECT_TYPE_DISPLAY_LIST, 0, dl);
  ASSERT_NE(list_handle, nullptr);
  auto picture = skity_picture_create_from_display_list(list_handle);
  skity_display_list_destroy(list_handle);
  ASSERT_NE(picture, nullptr);
  char filename[] = "/private/tmp/skity-core-review-picture-XXXXXX";
  int fd = mkstemp(filename);
  ASSERT_GE(fd, 0);
  close(fd);
  auto writer = skity_write_stream_create_from_file(filename);
  ASSERT_NE(writer, nullptr);
  skity_picture_serialize(picture, writer, nullptr);
  skity_write_stream_destroy(writer);
  skity_picture_destroy(picture);
  auto reader = skity_read_stream_create_from_file(filename);
  ASSERT_NE(reader, nullptr);
  auto restored = skity_picture_create_from_stream(reader);
  skity_read_stream_destroy(reader);
  std::remove(filename);
  ASSERT_NE(restored, nullptr);
  skity::Bitmap output(8, 8, skity::kPremul_AlphaType);
  std::shared_ptr<skity::Canvas> canvas =
      skity::Canvas::MakeSoftwareCanvas(&output);
  auto canvas_handle = skity::capi::alloc_handle<skity_canvas_s>(
      SKITY_OBJECT_TYPE_CANVAS, 0, canvas);
  ASSERT_NE(canvas_handle, nullptr);
  skity_picture_play_back(restored, canvas_handle);
  skity_canvas_destroy(canvas_handle);
  skity_picture_destroy(restored);
  auto expected = direct.GetPixel(4, 4);
  EXPECT_NEAR(ColorGetR(expected), 137, 2);
  ExpectRGBNear(output.GetPixel(4, 4), expected, 2);
}
