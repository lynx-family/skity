// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/effect/color_filter.hpp>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_check.hpp"

constexpr const char* kGoldenTestDir = CASE_DIR;

namespace {

struct PathListContext {
  explicit PathListContext(const char* name) : expected_path(kGoldenTestDir) {
    expected_path.append(name);
  }

  // Only the default tessellation path is exercised here: this case verifies
  // the gamma transfer math in the fragment stage, and other render variants
  // (gpu tessellation, coverage AA) only disagree on anti-aliased edges —
  // that rasterization noise is covered by the shape golden cases. Interior
  // pixels are identical across variants, and the color filter shader source
  // is shared by all pipeline variants.
  skity::testing::PathList ToPathList() const {
    return {
        .cpu_tess_path = expected_path.c_str(),
    };
  }

  std::filesystem::path expected_path;
};

// Gamma color filters must convert on straight (unpremultiplied) components,
// so translucent mid-tone coverage is required to exercise the difference
// between premultiplied and straight RGB on the GPU pipeline.
void DrawTranslucentOverlap(skity::Canvas* canvas,
                            std::shared_ptr<skity::ColorFilter> color_filter) {
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColorFilter(std::move(color_filter));

  paint.SetColor(skity::ColorSetARGB(128, 128, 64, 192));
  canvas->DrawCircle(70.f, 80.f, 50.f, paint);

  paint.SetColor(skity::ColorSetARGB(153, 64, 160, 96));
  canvas->DrawCircle(130.f, 120.f, 50.f, paint);
}

}  // namespace

TEST(ColorFilterGolden, SRGBToLinearGamma_Translucent) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  DrawTranslucentOverlap(recorder.GetRecordingCanvas(),
                         skity::ColorFilters::SRGBToLinearGamma());

  PathListContext context("gamma_srgb_to_linear_translucent.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ColorFilterGolden, LinearToSRGBGamma_Translucent) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  DrawTranslucentOverlap(recorder.GetRecordingCanvas(),
                         skity::ColorFilters::LinearToSRGBGamma());

  PathListContext context("gamma_linear_to_srgb_translucent.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}
