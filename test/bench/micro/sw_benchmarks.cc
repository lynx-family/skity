// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <benchmark/benchmark.h>

#include <skity/skity.hpp>

#include "case/basic/example.hpp"
#include "src/render/sw/sw_raster.hpp"
#include "src/render/sw/sw_span_brush.hpp"

static void BM_SWExamplePremulAlpha(benchmark::State& state) {
  skity::Bitmap bitmap(1000, 800, skity::AlphaType::kPremul_AlphaType);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  skity::Paint paint;
  paint.SetColor(skity::Color_WHITE);
  for (auto _ : state) {
    canvas->DrawPaint(paint);
    skity::example::basic::draw_canvas(canvas.get());
  }
}
BENCHMARK(BM_SWExamplePremulAlpha)->Unit(benchmark::kMicrosecond);

static void BM_SWExampleUnpremulAlpha(benchmark::State& state) {
  skity::Bitmap bitmap(1000, 800, skity::AlphaType::kUnpremul_AlphaType);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  skity::Paint paint;
  paint.SetColor(skity::Color_WHITE);
  for (auto _ : state) {
    canvas->DrawPaint(paint);
    skity::example::basic::draw_canvas(canvas.get());
  }
}
BENCHMARK(BM_SWExampleUnpremulAlpha)->Unit(benchmark::kMicrosecond);

static void BM_SWExampleUnpremulAlphaWithClip(benchmark::State& state) {
  skity::Bitmap bitmap(1000, 800, skity::AlphaType::kUnpremul_AlphaType);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  skity::Paint paint;
  paint.SetColor(skity::Color_WHITE);
  for (auto _ : state) {
    canvas->ClipRect(skity::Rect::MakeLTRB(100, 100, 200, 200));
    canvas->DrawPaint(paint);
    skity::example::basic::draw_canvas(canvas.get());
  }
}
BENCHMARK(BM_SWExampleUnpremulAlphaWithClip)->Unit(benchmark::kMicrosecond);

static void BM_SWRasterBigTriangle(benchmark::State& state) {
  skity::Bitmap bitmap(1000, 800, skity::AlphaType::kUnpremul_AlphaType);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  skity::Path path;
  path.MoveTo(500, 0);
  path.LineTo(0, 800);
  path.LineTo(1000, 800);
  path.Close();

  for (auto _ : state) {
    skity::SWRaster raster;
    raster.RastePath(path, skity::Matrix{});
  }
}
BENCHMARK(BM_SWRasterBigTriangle)->Unit(benchmark::kMicrosecond);

static void BM_SWRasterStar(benchmark::State& state) {
  skity::Bitmap bitmap(1000, 800, skity::AlphaType::kUnpremul_AlphaType);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  skity::Path star1;
  star1.MoveTo(100, 10);
  star1.LineTo(40, 180);
  star1.LineTo(190, 60);
  star1.LineTo(10, 60);
  star1.LineTo(160, 180);
  star1.Close();

  skity::Path star2 = star1;
  star2.SetFillType(skity::Path::PathFillType::kEvenOdd);

  for (auto _ : state) {
    skity::SWRaster raster1;
    raster1.RastePath(star1, skity::Matrix{});
    skity::SWRaster raster2;
    raster2.RastePath(star2, skity::Matrix{});
  }
}
BENCHMARK(BM_SWRasterStar)->Unit(benchmark::kMicrosecond);

static void BM_SWDrawBigImage(benchmark::State& state) {
  skity::Bitmap bitmap1(1000, 800, skity::AlphaType::kPremul_AlphaType);
  auto canvas1 = skity::Canvas::MakeSoftwareCanvas(&bitmap1);

  skity::Paint paint;
  paint.SetColor(skity::Color_WHITE);
  canvas1->DrawPaint(paint);
  skity::example::basic::draw_canvas(canvas1.get());

  skity::Bitmap bitmap2(1000, 800, skity::AlphaType::kPremul_AlphaType);
  auto canvas2 = skity::Canvas::MakeSoftwareCanvas(&bitmap2);
  std::shared_ptr<skity::Image> image =
      skity::Image::MakeImage(bitmap1.GetPixmap());

  for (auto _ : state) {
    canvas2->DrawImage(image,
                       skity::Rect::MakeWH(image->Width(), image->Height()));
  }
}
BENCHMARK(BM_SWDrawBigImage)->Unit(benchmark::kMicrosecond);

static void BM_SWDrawBigImageLinear(benchmark::State& state) {
  skity::Bitmap bitmap1(1000, 800, skity::AlphaType::kPremul_AlphaType);
  auto canvas1 = skity::Canvas::MakeSoftwareCanvas(&bitmap1);

  skity::Paint paint;
  paint.SetColor(skity::Color_WHITE);
  canvas1->DrawPaint(paint);
  skity::example::basic::draw_canvas(canvas1.get());

  skity::Bitmap bitmap2(1000, 800, skity::AlphaType::kPremul_AlphaType);
  auto canvas2 = skity::Canvas::MakeSoftwareCanvas(&bitmap2);
  std::shared_ptr<skity::Image> image =
      skity::Image::MakeImage(bitmap1.GetPixmap());

  skity::SamplingOptions options{skity::FilterMode::kLinear,
                                 skity::MipmapMode::kNone};

  for (auto _ : state) {
    canvas2->DrawImage(
        image, skity::Rect::MakeWH(image->Width(), image->Height()), options);
  }
}
BENCHMARK(BM_SWDrawBigImageLinear)->Unit(benchmark::kMicrosecond);

static void BM_SWDrawBigImageWithBlur(benchmark::State& state) {
  skity::Bitmap bitmap1(1000, 800, skity::AlphaType::kPremul_AlphaType);
  auto canvas1 = skity::Canvas::MakeSoftwareCanvas(&bitmap1);

  skity::Paint paint;
  paint.SetColor(skity::Color_WHITE);
  canvas1->DrawPaint(paint);
  skity::example::basic::draw_canvas(canvas1.get());

  skity::Bitmap bitmap2(1000, 800, skity::AlphaType::kPremul_AlphaType);
  auto canvas2 = skity::Canvas::MakeSoftwareCanvas(&bitmap2);
  std::shared_ptr<skity::Image> image =
      skity::Image::MakeImage(bitmap1.GetPixmap());
  auto mask_filter = skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 20);
  paint.SetMaskFilter(mask_filter);
  for (auto _ : state) {
    canvas2->DrawImage(
        image, skity::Rect::MakeWH(image->Width(), image->Height()), &paint);
  }
}
BENCHMARK(BM_SWDrawBigImageWithBlur)->Unit(benchmark::kMicrosecond);

class GradientSpanTest : public skity::GradientColorBrush {
 public:
  GradientSpanTest(skity::Shader::GradientInfo info,
                   skity::Shader::GradientType type)
      : GradientColorBrush(std::vector<skity::Span>{}, nullptr, nullptr,
                           skity::BlendMode::kDefault, std::move(info), type) {}

  ~GradientSpanTest() override = default;

  void TestLoop(float x, float y) { this->CalculateColor(x, y); }
};

static void BM_SWGradientSpanBrush(benchmark::State& state) {
  skity::Vec4 colors[] = {
      skity::Vec4{0.f, 1.f, 1.f, 0.f},
      skity::Vec4{0.f, 0.f, 1.f, 1.f},
      skity::Vec4{1.f, 0.f, 0.f, 1.f},
  };
  float positions[] = {0.f, 0.65f, 1.f};

  float blockX = 100.f;
  float blockY = 100.f;

  std::vector<skity::Point> pts = {
      skity::Point{blockX, blockY, 0.f, 1.f},
      skity::Point{blockX + 50, blockY + 100, 0.f, 1.f},
  };

  auto lgs = skity::Shader::MakeLinear(pts.data(), colors, positions, 3,
                                       skity::TileMode::kClamp);

  skity::Shader::GradientInfo info;

  auto type = lgs->AsGradient(&info);

  GradientSpanTest test_brush(info, type);

  for (auto _ : state) {
    for (size_t i = 0; i < 500 * 500; i++) {
      test_brush.TestLoop(105.f, 105.f);
    }
  }
}

BENCHMARK(BM_SWGradientSpanBrush)->Unit(benchmark::kMicrosecond);
