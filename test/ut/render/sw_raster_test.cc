// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_raster.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/path.hpp>
#include <vector>

namespace {

// A small closed outline with one curve, shaped like a glyph contour.
skity::Path GlyphLikePath() {
  skity::Path path;
  path.MoveTo(1.f, 1.f);
  path.LineTo(9.f, 1.f);
  path.QuadTo(11.f, 5.f, 9.f, 9.f);
  path.LineTo(1.f, 9.f);
  path.Close();
  return path;
}

// Coordinates a 16.16 fixed-point scanline raster can represent after the
// edge builder's extra two accuracy bits.
constexpr int32_t kFixedSafeLimit = 8192;

bool SpansWithin(const std::vector<skity::Span>& spans, int32_t left,
                 int32_t top, int32_t right, int32_t bottom) {
  for (const skity::Span& span : spans) {
    if (span.y < top || span.y >= bottom || span.x < left ||
        span.x + span.len > right || span.len <= 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(SWRaster, FarOffscreenRotatedPathProducesNoSpansInsideClip) {
  skity::SWRaster raster;
  const skity::Matrix transform =
      skity::Matrix::Translate(8.f, -120000.f) * skity::Matrix::RotateDeg(45.f);
  raster.RastePath(GlyphLikePath(), transform,
                   skity::Rect::MakeWH(1024.f, 768.f));
  EXPECT_TRUE(raster.CurrentSpans().empty());
}

TEST(SWRaster, FarOffscreenRotatedPathWithDefaultCullStaysInFixedRange) {
  skity::SWRaster raster;
  const skity::Matrix transform =
      skity::Matrix::Translate(8.f, -120000.f) * skity::Matrix::RotateDeg(45.f);
  raster.RastePath(GlyphLikePath(), transform);
  EXPECT_TRUE(SpansWithin(raster.CurrentSpans(), -kFixedSafeLimit,
                          -kFixedSafeLimit, kFixedSafeLimit, kFixedSafeLimit));
}

TEST(SWRaster, HugePartiallyVisibleTriangleIsClippedToScanBounds) {
  skity::Path path;
  path.MoveTo(10.f, 10.f);
  path.LineTo(200000.f, 150000.f);
  path.LineTo(-100000.f, 120000.f);
  path.Close();
  skity::SWRaster raster;
  raster.RastePath(path, skity::Matrix{}, skity::Rect::MakeWH(64.f, 64.f));
  EXPECT_FALSE(raster.CurrentSpans().empty());
  EXPECT_TRUE(SpansWithin(raster.CurrentSpans(), 0, 0, 64, 64));
}

TEST(SWRaster, HugeCurvedPathIsClippedToScanBounds) {
  skity::Path path;
  path.MoveTo(-50000.f, 20.f);
  path.QuadTo(30.f, -90000.f, 70000.f, 40.f);
  path.LineTo(20.f, 150000.f);
  path.Close();
  skity::SWRaster raster;
  raster.RastePath(path, skity::Matrix{}, skity::Rect::MakeWH(64.f, 64.f));
  EXPECT_TRUE(SpansWithin(raster.CurrentSpans(), 0, 0, 64, 64));
}

TEST(SWRaster, ClippedVisiblePortionMatchesUnclippedRaster) {
  // A rectangle that extends beyond the clip on every side must cover the
  // whole clip exactly as an equivalent in-range rectangle does.
  skity::Path huge;
  huge.AddRect(skity::Rect::MakeLTRB(-100000.f, -100000.f, 100000.f, 100000.f));
  skity::Path fitted;
  fitted.AddRect(skity::Rect::MakeLTRB(-4.f, -4.f, 20.f, 20.f));
  skity::SWRaster huge_raster;
  huge_raster.RastePath(huge, skity::Matrix{}, skity::Rect::MakeWH(16.f, 16.f));
  skity::SWRaster fitted_raster;
  fitted_raster.RastePath(fitted, skity::Matrix{},
                          skity::Rect::MakeWH(16.f, 16.f));
  std::array<int32_t, 16 * 16> huge_cover{};
  std::array<int32_t, 16 * 16> fitted_cover{};
  const auto accumulate = [](const std::vector<skity::Span>& spans,
                             std::array<int32_t, 16 * 16>& cover) {
    for (const skity::Span& span : spans) {
      for (int32_t x = span.x; x < span.x + span.len; ++x) {
        if (span.y >= 0 && span.y < 16 && x >= 0 && x < 16) {
          cover[static_cast<size_t>(span.y * 16 + x)] += span.cover;
        }
      }
    }
  };
  accumulate(huge_raster.CurrentSpans(), huge_cover);
  accumulate(fitted_raster.CurrentSpans(), fitted_cover);
  EXPECT_EQ(huge_cover, fitted_cover);
  EXPECT_EQ(huge_cover[0], 255);
  EXPECT_EQ(huge_cover[16 * 16 - 1], 255);
}

namespace {

// Coverage of one row-clipped raster, written into `rows`. A row a clip owns
// must hold what the unclipped raster holds there.
void AccumulateClipRows(const skity::Path& path, const skity::Matrix& transform,
                        const skity::Rect& clip,
                        std::vector<std::vector<int32_t>>& rows) {
  skity::SWRaster raster;
  raster.RastePath(path, transform, clip);
  for (const skity::Span& span : raster.CurrentSpans()) {
    ASSERT_GE(span.y, clip.Top());
    ASSERT_LT(span.y, clip.Bottom());
    auto& row = rows[static_cast<size_t>(span.y)];
    for (int32_t x = span.x; x < span.x + span.len; ++x) {
      row[static_cast<size_t>(x)] += span.cover;
    }
  }
}

void ExpectClipsMatchWhole(const skity::Path& path,
                           const skity::Matrix& transform, int32_t size,
                           int32_t rows_per_clip) {
  const auto blank = std::vector<std::vector<int32_t>>(
      static_cast<size_t>(size),
      std::vector<int32_t>(static_cast<size_t>(size)));
  auto whole = blank;
  const float extent = static_cast<float>(size);
  AccumulateClipRows(path, transform, skity::Rect::MakeWH(extent, extent),
                     whole);
  auto clipped = blank;
  for (int32_t top = 0; top < size; top += rows_per_clip) {
    AccumulateClipRows(
        path, transform,
        skity::Rect::MakeLTRB(0.f, static_cast<float>(top), extent,
                              static_cast<float>(top + rows_per_clip)),
        clipped);
  }
  for (int32_t y = 0; y < size; ++y) {
    EXPECT_EQ(whole[static_cast<size_t>(y)], clipped[static_cast<size_t>(y)])
        << "row " << y << " with " << rows_per_clip << " rows per clip";
  }
}

}  // namespace

TEST(SWRaster, RowClipsMatchWholeRasterForSlantedLines) {
  // A line that crosses a clip keeps its own slope instead of restarting at the
  // clip, where the fixed-point slope would round from a different point.
  skity::Path path;
  path.MoveTo(3.3f, 2.7f);
  path.LineTo(91.1f, 13.9f);
  path.LineTo(37.7f, 94.3f);
  path.Close();
  for (const int32_t rows_per_clip : {1, 7, 16, 32}) {
    ExpectClipsMatchWhole(path, skity::Matrix{}, 96, rows_per_clip);
  }
}

TEST(SWRaster, RowClipsMatchWholeRasterUnderScaleAndRotation) {
  skity::Path path;
  path.AddRoundRect(skity::Rect::MakeLTRB(4.f, 4.f, 44.f, 30.f), 9.f, 9.f);
  const skity::Matrix transform = skity::Matrix::Translate(48.f, 18.f) *
                                  skity::Matrix::RotateDeg(27.f) *
                                  skity::Matrix::Scale(1.5f, 1.25f);
  for (const int32_t rows_per_clip : {1, 7, 16, 32}) {
    ExpectClipsMatchWhole(path, transform, 128, rows_per_clip);
  }
}

TEST(SpanBuilder, HorizontalWritesAreClampedToTheCoverageRow) {
  skity::SpanBuilder builder(0, 8, skity::Rect::MakeWH(8.f, 1.f), nullptr);
  std::array<uint8_t, 16> antialias{};
  antialias.fill(255);
  builder.BuildSpans(-4, 0, antialias.data(), 16);
  builder.BuildSpan(-2, 0, 255);
  builder.BuildSpan(6, 0, 8, 255);
  builder.Flush();
  const std::vector<skity::Span> spans = builder.TakeSpans();
  EXPECT_FALSE(spans.empty());
  EXPECT_TRUE(SpansWithin(spans, 0, 0, 8, 1));
}
