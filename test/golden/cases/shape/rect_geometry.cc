// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <skity/skity.hpp>

#include "common/golden_test_check.hpp"
#include "common/golden_test_env.hpp"

namespace skity {
namespace {

enum class TestAA { kMSAA, kContour, kCoverage };

std::shared_ptr<Pixmap> RenderScene(uint32_t width, uint32_t height, TestAA aa,
                                    bool merge,
                                    const std::function<void(Canvas*)>& draw,
                                    const char* golden_path = nullptr) {
  auto* env = testing::GoldenTestEnv::GetInstance();
  auto* context = env->GetGPUContext();
  const auto old_samples = env->GetSampleCount();
  const bool old_merge = context->IsEnableMergingDrawCall();
  const bool old_contour_aa = context->IsEnableContourAA();
  const bool old_coverage_aa = context->IsEnableCoverageAA();
  const bool old_simple_shapes = context->IsEnableSimpleShapePipeline();
  env->SetSampleCount(aa == TestAA::kMSAA ? 4 : 1);
  context->SetEnableMergingDrawCall(merge);
  context->SetEnableContourAA(aa == TestAA::kContour);
  context->SetEnableCoverageAA(aa == TestAA::kCoverage);
  context->SetEnableSimpleShapePipeline(true);

  auto texture = env->RenderToTexture(width, height, [&](Canvas* canvas) {
    canvas->Clear(Color_BLACK);
    draw(canvas);
  });
  std::shared_ptr<Pixmap> pixels;
  if (golden_path != nullptr) {
    EXPECT_TRUE(testing::CompareGoldenTexture(texture, golden_path, &pixels));
  } else if (texture != nullptr) {
    pixels = texture->ReadPixels();
  }

  env->SetSampleCount(old_samples);
  context->SetEnableMergingDrawCall(old_merge);
  context->SetEnableContourAA(old_contour_aa);
  context->SetEnableCoverageAA(old_coverage_aa);
  context->SetEnableSimpleShapePipeline(old_simple_shapes);
  return pixels;
}

void DrawRectAsPath(Canvas* canvas, const Rect& rect, const Paint& paint) {
  // Explicit edges keep the reference on the general path pipeline.
  Path path;
  path.MoveTo(rect.Left(), rect.Top());
  path.LineTo(rect.Right(), rect.Top());
  path.LineTo(rect.Right(), rect.Bottom());
  path.LineTo(rect.Left(), rect.Bottom());
  path.Close();
  canvas->DrawPath(path, paint);
}

std::shared_ptr<Pixmap> RenderRectScene(uint32_t samples, bool merge,
                                        bool use_path_reference,
                                        const char* golden_path = nullptr) {
  const auto aa = samples > 1 ? TestAA::kMSAA : TestAA::kContour;
  auto draw_scene = [&](Canvas* canvas) {
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
        DrawRectAsPath(canvas, rect, paint);
      } else {
        canvas->DrawRect(rect, paint);
      }
    };

    draw(Rect::MakeLTRB(2, 2, 12, 12), ColorSetARGB(128, 255, 0, 0));
    canvas->Save();
    canvas->Translate(16.f, 4.f);
    canvas->Scale(-1.f, 2.f);
    if (samples > 1) {
      canvas->Concat(Matrix::Skew(0.25f, -0.25f));
    }
    draw(Rect::MakeWH(10, 6), ColorSetARGB(128, 0, 0, 255));
    canvas->Restore();
    draw(Rect::MakeXYWH(14, 2, samples > 1 ? 0.5f : 1.f, 4), Color_WHITE);
  };
  return RenderScene(24, 24, aa, merge, draw_scene, golden_path);
}

TEST(RectGeometryGolden, AffineRectsMatchPathWithMSAA) {
  const Rect rect = Rect::MakeLTRB(-5.25f, -3.75f, 11.125f, 9.25f);
  const struct {
    const char* golden_path;
    Matrix transform;
  } cases[] = {
      {CASE_DIR "rect_geometry/msaa_rotation.png",
       Matrix::Translate(20.375f, 17.125f) * Matrix::RotateDeg(27.f)},
      {CASE_DIR "rect_geometry/msaa_skew.png",
       Matrix::Translate(17.125f, 16.375f) * Matrix::Skew(0.4375f, -0.1875f)},
      {CASE_DIR "rect_geometry/msaa_rotation_skew_reflection.png",
       Matrix::Translate(39.375f, 18.625f) * Matrix::RotateDeg(-27.f) *
           Matrix::Skew(0.25f, 0.f) * Matrix::Scale(-1.25f, 0.75f)},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.golden_path);
    auto render = [&](bool use_path_reference) {
      auto draw = [&](Canvas* canvas) {
        canvas->SetMatrix(test.transform);
        Paint paint;
        paint.SetAntiAlias(true);
        paint.SetColor(Color_WHITE);
        if (use_path_reference) {
          DrawRectAsPath(canvas, rect, paint);
        } else {
          canvas->DrawRect(rect, paint);
        }
      };
      return RenderScene(56, 48, TestAA::kMSAA, false, draw,
                         use_path_reference ? nullptr : test.golden_path);
    };

    auto reference = render(true);
    auto actual = render(false);
    ASSERT_NE(reference, nullptr);
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(testing::ComparePixelsExact(reference, actual).diff_pixel_count,
              0u);

    Bitmap bitmap(actual);
    EXPECT_EQ(bitmap.GetPixel(static_cast<int>(test.transform.GetTranslateX()),
                              static_cast<int>(test.transform.GetTranslateY())),
              Color_WHITE);
    bool has_partial_coverage = false;
    for (uint32_t y = 0; y < actual->Height(); ++y) {
      for (uint32_t x = 0; x < actual->Width(); ++x) {
        const auto pixel = bitmap.GetPixel(x, y);
        has_partial_coverage |= pixel != Color_BLACK && pixel != Color_WHITE;
      }
    }
    EXPECT_TRUE(has_partial_coverage);
  }
}

TEST(RectGeometryGolden, PixelAlignedRectsWithAnalyticalAA) {
  for (auto aa : {TestAA::kContour, TestAA::kCoverage}) {
    SCOPED_TRACE(aa == TestAA::kContour ? "Contour AA" : "Coverage AA");
    auto draw = [](Canvas* canvas) {
      Paint paint;
      paint.SetAntiAlias(true);
      paint.SetColor(Color_GREEN);
      canvas->SetMatrix(Matrix::Translate(4.f, 7.f) * Matrix::Scale(2.f, 4.f));
      canvas->DrawRect(Rect::MakeLTRB(0.5f, 0.25f, 7.5f, 4.25f), paint);

      Matrix quarter_turn = Matrix::Scale(0.f, 0.f);
      quarter_turn.SetSkewX(-1.f);
      quarter_turn.SetSkewY(1.f);
      quarter_turn.SetTranslateX(42.f);
      quarter_turn.SetTranslateY(4.f);
      canvas->SetMatrix(quarter_turn);
      paint.SetColor(Color_BLUE);
      // The transformed right edge is within the pixel alignment tolerance.
      canvas->DrawRect(Rect::MakeLTRB(4.f, 3.0001f, 14.f, 11.f), paint);
    };
    auto actual =
        RenderScene(48, 32, aa, false, draw,
                    CASE_DIR "rect_geometry/pixel_aligned_analytical_aa.png");
    ASSERT_NE(actual, nullptr);
    Bitmap bitmap(actual);
    const Rect green = Rect::MakeLTRB(5.f, 8.f, 19.f, 24.f);
    const Rect blue = Rect::MakeLTRB(31.f, 8.f, 39.f, 18.f);
    for (uint32_t y = 0; y < actual->Height(); ++y) {
      for (uint32_t x = 0; x < actual->Width(); ++x) {
        Color expected = Color_BLACK;
        if (green.Contains(x + 0.5f, y + 0.5f)) {
          expected = Color_GREEN;
        } else if (blue.Contains(x + 0.5f, y + 0.5f)) {
          expected = Color_BLUE;
        }
        ASSERT_EQ(bitmap.GetPixel(x, y), expected)
            << "pixel " << x << ", " << y;
      }
    }
  }
}

TEST(RectGeometryGolden, BatchAndPathHaveTheSamePixels) {
  for (uint32_t samples : {1u, 4u}) {
    SCOPED_TRACE(samples);
    auto reference = RenderRectScene(samples, false, true);
    auto separate = RenderRectScene(samples, false, false);
    const char* golden_path =
        samples > 1 ? CASE_DIR "rect_geometry/rect_batch_msaa.png"
                    : CASE_DIR "rect_geometry/rect_batch_contour_aa.png";
    auto batched = RenderRectScene(samples, true, false, golden_path);
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

TEST(RectGeometryGolden, RectAndRRectBatchesPreserveDrawOrder) {
  for (auto aa : {TestAA::kContour, TestAA::kMSAA}) {
    SCOPED_TRACE(aa == TestAA::kMSAA ? "MSAA" : "Contour AA");
    auto render = [&](bool merge) {
      auto draw = [](Canvas* canvas) {
        Paint paint;
        paint.SetAntiAlias(true);
        paint.SetColor(ColorSetARGB(128, 255, 0, 0));
        canvas->DrawRRect(RRect::MakeRectXY({3, 3, 21, 21}, 4, 4), paint);

        paint.SetColor(ColorSetARGB(128, 0, 255, 0));
        canvas->DrawRect(Rect::MakeLTRB(8, 5, 26, 24), paint);
        paint.SetColor(ColorSetARGB(128, 0, 0, 255));
        canvas->DrawRect(Rect::MakeLTRB(14, 9, 33, 27), paint);

        paint.SetColor(ColorSetARGB(128, 255, 255, 0));
        canvas->DrawRRect(RRect::MakeRectXY({11, 9, 31, 25}, 5, 4), paint);
        canvas->Save();
        canvas->Translate(19.f, 11.f);
        canvas->Scale(1.25f, 1.f);
        paint.SetColor(ColorSetARGB(128, 255, 0, 255));
        canvas->DrawRRect(RRect::MakeRectXY(Rect::MakeWH(17, 14), 4, 2), paint);
        canvas->Restore();

        paint.SetColor(Color_WHITE);
        canvas->DrawRect(Rect::MakeLTRB(13, 12, 29, 23), paint);
      };
      const char* golden_path =
          merge ? CASE_DIR "rect_geometry/rect_rrect_batch.png" : nullptr;
      return RenderScene(48, 36, aa, merge, draw, golden_path);
    };

    auto separate = render(false);
    auto batched = render(true);
    ASSERT_NE(separate, nullptr);
    ASSERT_NE(batched, nullptr);
    EXPECT_EQ(testing::ComparePixelsExact(separate, batched).diff_pixel_count,
              0u);
    EXPECT_EQ(Bitmap(batched).GetPixel(20, 16), Color_WHITE);
  }
}

TEST(RectGeometryGolden, CoverageAAKeepsSharedPixelOrder) {
  auto render = [](bool merge) {
    auto draw = [](Canvas* canvas) {
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
    };
    const char* golden_path =
        merge ? CASE_DIR "rect_geometry/coverage_aa_shared_pixel_order.png"
              : nullptr;
    return RenderScene(24, 24, TestAA::kCoverage, merge, draw, golden_path);
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
