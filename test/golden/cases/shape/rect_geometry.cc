// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <memory>
#include <skity/skity.hpp>

#include "common/golden_test_check.hpp"
#include "common/golden_test_env.hpp"

namespace skity {
namespace {

std::shared_ptr<Pixmap> RenderRectScene(uint32_t samples, bool merge,
                                        bool use_path_reference) {
  auto* env = testing::GoldenTestEnv::GetInstance();
  auto* context = env->GetGPUContext();
  const auto old_samples = env->GetSampleCount();
  const bool old_merge = context->IsEnableMergingDrawCall();
  const bool old_contour_aa = context->IsEnableContourAA();
  env->SetSampleCount(samples);
  context->SetEnableMergingDrawCall(merge);
  context->SetEnableContourAA(true);

  auto texture = env->RenderToTexture(24, 24, [&](Canvas* canvas) {
    canvas->Clear(Color_BLACK);

    Path clip;
    clip.MoveTo(1.f, 1.f);
    clip.LineTo(20.f, 1.f);
    clip.LineTo(1.f, 20.f);
    clip.Close();
    canvas->ClipPath(clip);
    if (samples > 1) {
      canvas->Translate(0.25f, 0.25f);
    }

    Paint paint;
    paint.SetAntiAlias(!use_path_reference);
    auto draw = [&](const Rect& rect, Color color) {
      paint.SetColor(color);
      if (use_path_reference) {
        Path path;
        path.MoveTo(rect.Left(), rect.Top());
        path.LineTo(rect.Right(), rect.Top());
        path.LineTo(rect.Right(), rect.Bottom());
        path.LineTo(rect.Left(), rect.Bottom());
        path.Close();
        canvas->DrawPath(path, paint);
      } else {
        canvas->DrawRect(rect, paint);
      }
    };

    draw(Rect::MakeLTRB(2, 2, 12, 12), ColorSetARGB(128, 255, 0, 0));
    canvas->Save();
    canvas->Translate(16.f, 4.f);
    canvas->Scale(-1.f, 2.f);
    draw(Rect::MakeWH(10, 6), ColorSetARGB(128, 0, 0, 255));
    canvas->Restore();
    draw(Rect::MakeXYWH(14, 2, samples > 1 ? 0.5f : 1.f, 4), Color_WHITE);
  });

  env->SetSampleCount(old_samples);
  context->SetEnableMergingDrawCall(old_merge);
  context->SetEnableContourAA(old_contour_aa);
  return texture ? texture->ReadPixels() : nullptr;
}

TEST(RectGeometryGolden, BatchAndPathHaveTheSamePixels) {
  for (uint32_t samples : {1u, 4u}) {
    SCOPED_TRACE(samples);
    auto reference = RenderRectScene(samples, false, true);
    auto separate = RenderRectScene(samples, false, false);
    auto batched = RenderRectScene(samples, true, false);
    ASSERT_NE(reference, nullptr);
    ASSERT_NE(separate, nullptr);
    ASSERT_NE(batched, nullptr);

    const auto batch_diff = testing::ComparePixelsExact(separate, batched);
    EXPECT_TRUE(batch_diff.passed);
    EXPECT_EQ(batch_diff.diff_pixel_count, 0u);
    const auto reference_diff = testing::ComparePixelsExact(reference, batched);
    EXPECT_TRUE(reference_diff.passed);
    EXPECT_EQ(reference_diff.diff_pixel_count, 0u);

    Bitmap bitmap(batched);
    EXPECT_EQ(bitmap.GetPixel(3, 3), ColorSetARGB(255, 128, 0, 0));
    EXPECT_EQ(bitmap.GetPixel(8, 6), ColorSetARGB(255, 64, 0, 128));
    EXPECT_EQ(bitmap.GetPixel(18, 18), Color_BLACK);
  }
}

TEST(RectGeometryGolden, CoverageAAKeepsSharedPixelOrder) {
  auto render = [](bool merge) {
    auto* env = testing::GoldenTestEnv::GetInstance();
    auto* context = env->GetGPUContext();
    const auto old_samples = env->GetSampleCount();
    const bool old_merge = context->IsEnableMergingDrawCall();
    const bool old_coverage_aa = context->IsEnableCoverageAA();
    env->SetSampleCount(1);
    context->SetEnableMergingDrawCall(merge);
    context->SetEnableCoverageAA(true);

    auto texture = env->RenderToTexture(24, 24, [](Canvas* canvas) {
      canvas->Clear(Color_BLACK);
      Paint blue;
      blue.SetAntiAlias(false);
      blue.SetColor(Color_BLUE);
      canvas->DrawRect(Rect::MakeLTRB(1.f, 1.f, 3.f, 3.f), blue);
      Paint green;
      green.SetAntiAlias(true);
      green.SetColor(Color_GREEN);
      // These two disjoint rectangles both contribute to pixel (8, 6).
      canvas->DrawRect(Rect::MakeLTRB(8.75f, 4.f, 15.f, 13.f), green);
      canvas->DrawRect(Rect::MakeLTRB(4.f, 4.f, 8.625f, 13.f), blue);
    });

    env->SetSampleCount(old_samples);
    context->SetEnableMergingDrawCall(old_merge);
    context->SetEnableCoverageAA(old_coverage_aa);
    return texture ? texture->ReadPixels() : nullptr;
  };

  auto separate = render(false);
  auto batched = render(true);
  ASSERT_NE(separate, nullptr);
  ASSERT_NE(batched, nullptr);
  EXPECT_EQ(Bitmap(separate).GetPixel(8, 6), Color_BLUE);
  EXPECT_EQ(Bitmap(batched).GetPixel(8, 6), Color_BLUE);
  const auto diff = testing::ComparePixelsExact(separate, batched);
  EXPECT_TRUE(diff.passed);
  EXPECT_EQ(diff.diff_pixel_count, 0u);
}

}  // namespace
}  // namespace skity
