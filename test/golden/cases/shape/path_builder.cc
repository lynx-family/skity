// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <skity/c/skity_canvas.h>
#include <skity/c/skity_paint.h>
#include <skity/c/skity_path_builder.h>

#include <skity/hpp/skity_bridge.hpp>
#include <skity/skity.hpp>

#include "common/golden_test_check.hpp"
#include "common/golden_test_env.hpp"

namespace {
using namespace skity;

struct ScopedPathAA {
  skity::testing::GoldenTestEnv* env;
  uint32_t samples = env->GetSampleCount();
  GPUContext* gpu = env->GetGPUContext();
  bool contour = gpu->IsEnableContourAA();
  bool coverage = gpu->IsEnableCoverageAA();
  bool simple = gpu->IsEnableSimpleShapePipeline();
  bool tessellation = gpu->IsEnableGPUTessellation();
  ~ScopedPathAA() {
    env->SetSampleCount(samples);
    gpu->SetEnableContourAA(contour);
    gpu->SetEnableCoverageAA(coverage);
    gpu->SetEnableSimpleShapePipeline(simple);
    gpu->SetEnableGPUTessellation(tessellation);
  }
};

template <typename Target>
void BuildShape(Target& path) {
  path.MoveTo(12.5f, 18.5f);
  path.CubicTo(35, 2, 75, 110, 110, 20);
  path.ConicTo(100, 100, 20, 105, 0.7f);
  path.QuadTo(35, 55, 12.5f, 18.5f).Close();
  path.AddOval({40, 35, 65, 70}, Path::Direction::kCCW);
}

void CompareBuilderRendering(bool coverage) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  if (!env || env->GetBackend() != skity::testing::Backend::kGL) {
    GTEST_SKIP() << "Requires GL";
  }
  ScopedPathAA restore{env};
  env->SetSampleCount(1);
  auto* gpu = env->GetGPUContext();
  gpu->SetEnableContourAA(!coverage);
  gpu->SetEnableCoverageAA(coverage);
  gpu->SetEnableSimpleShapePipeline(false);
  gpu->SetEnableGPUTessellation(false);

  Path legacy;
  BuildShape(legacy);
  PathBuilder builder;
  BuildShape(builder);
  Path built = builder.Detach();
  std::unique_ptr<skity_path_builder_s, decltype(&skity_path_builder_destroy)>
      c_builder(skity_path_builder_create(), skity_path_builder_destroy);
  ASSERT_NE(c_builder, nullptr);
  skity_path_builder_move_to(c_builder.get(), 12.5f, 18.5f);
  skity_path_builder_cubic_to(c_builder.get(), 35, 2, 75, 110, 110, 20);
  skity_path_builder_conic_to(c_builder.get(), 100, 100, 20, 105, 0.7f);
  skity_path_builder_quad_to(c_builder.get(), 35, 55, 12.5f, 18.5f);
  skity_path_builder_close(c_builder.get());
  skity_rect oval{40, 35, 65, 70};
  skity_path_builder_add_oval(c_builder.get(), &oval, SKITY_PATH_DIRECTION_CCW);
  std::unique_ptr<skity_path_s, decltype(&skity_path_destroy)> c_path(
      skity_path_builder_detach(c_builder.get()), skity_path_destroy);
  ASSERT_NE(c_path, nullptr);
  c_builder.reset();

  for (auto fill :
       {Path::PathFillType::kWinding, Path::PathFillType::kEvenOdd}) {
    for (auto style : {Paint::kFill_Style, Paint::kStroke_Style}) {
      legacy.SetFillType(fill);
      built.SetFillType(fill);
      skity_path_set_fill_type(c_path.get(),
                               static_cast<skity_path_fill_type>(fill));
      Paint paint;
      paint.SetAntiAlias(true);
      paint.SetColor(Color_WHITE);
      paint.SetStyle(style);
      paint.SetStrokeWidth(3.5f);
      std::unique_ptr<skity_paint_s, decltype(&skity_paint_destroy)> c_paint(
          skity_paint_create(), skity_paint_destroy);
      ASSERT_NE(c_paint, nullptr);
      skity_paint_set_anti_alias(c_paint.get(), 1);
      skity_paint_set_color(c_paint.get(), Color_WHITE);
      skity_paint_set_style(c_paint.get(),
                            static_cast<skity_paint_style>(style));
      skity_paint_set_stroke_width(c_paint.get(), 3.5f);
      auto render = [&](int source) {
        return env->RenderToTexture(128, 128, [&](Canvas* canvas) {
          canvas->Clear(Color_TRANSPARENT);
          canvas->ClipRRect(RRect::MakeRectXY({4, 4, 124, 124}, 9, 11));
          if (source < 2) {
            canvas->DrawPath(source == 0 ? legacy : built, paint);
          } else {
            auto c_canvas = skity_canvas_from_native(canvas);
            skity_canvas_draw_path(c_canvas, c_path.get(), c_paint.get());
            skity_canvas_destroy(c_canvas);
          }
        });
      };
      auto expected_texture = render(0);
      ASSERT_NE(expected_texture, nullptr);
      auto expected = expected_texture->ReadPixels();
      ASSERT_NE(expected, nullptr);
      bool has_coverage = false;
      for (uint32_t y = 0; y < expected->Height(); ++y) {
        for (uint32_t x = 0; x < expected->Width(); ++x) {
          has_coverage |= expected->Addr8(x, y)[3] != 0;
        }
      }
      ASSERT_TRUE(has_coverage);
      for (int source : {1, 2}) {
        SCOPED_TRACE(::testing::Message() << "C API=" << (source == 2)
                                          << " fill=" << static_cast<int>(fill)
                                          << " style=" << style);
        auto texture = render(source);
        ASSERT_NE(texture, nullptr);
        auto actual = texture->ReadPixels();
        ASSERT_NE(actual, nullptr);
        auto diff = skity::testing::ComparePixelsExact(expected, actual);
        EXPECT_EQ(diff.diff_pixel_count, 0);
      }
    }
  }
}

TEST(PathBuilderGL, ContourAAAtOneSample) { CompareBuilderRendering(false); }
TEST(PathBuilderGL, CoverageAAAtOneSample) { CompareBuilderRendering(true); }
}  // namespace
