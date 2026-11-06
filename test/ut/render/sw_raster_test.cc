// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_raster.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/path.hpp>
#include <vector>

namespace {

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

}  // namespace

TEST(SWRaster, SmallCircleCoverageFollowsTheCurve) {
  // A chord cuts inside a convex curve, so too few segments lose coverage on
  // every edge pixel of a small corner. Compare with the true circle, on the
  // sides where the edge is steep: edge rows snap to quarter pixels, so the
  // shallow top and bottom carry that larger error instead.
  constexpr int32_t kSize = 24;
  constexpr float kCenterX = 12.25f;
  constexpr float kCenterY = 12.5f;
  constexpr float kRadius = 8.f;
  skity::Path path;
  path.AddCircle(kCenterX, kCenterY, kRadius);
  std::vector<std::vector<int32_t>> rows(
      kSize, std::vector<int32_t>(static_cast<size_t>(kSize)));
  AccumulateClipRows(
      path, skity::Matrix{},
      skity::Rect::MakeWH(static_cast<float>(kSize), static_cast<float>(kSize)),
      rows);
  constexpr int kSamples = 32;
  int32_t worst = 0;
  for (int32_t y = 0; y < kSize; ++y) {
    for (int32_t x = 0; x < kSize; ++x) {
      int inside = 0;
      for (int sy = 0; sy < kSamples; ++sy) {
        for (int sx = 0; sx < kSamples; ++sx) {
          const float dx = x + (sx + .5f) / kSamples - kCenterX;
          const float dy = y + (sy + .5f) / kSamples - kCenterY;
          inside += dx * dx + dy * dy <= kRadius * kRadius;
        }
      }
      const int32_t expected = inside * 255 / (kSamples * kSamples);
      if (std::abs(x + .5f - kCenterX) < std::abs(y + .5f - kCenterY)) continue;
      worst = std::max(
          worst, std::abs(rows[static_cast<size_t>(y)][static_cast<size_t>(x)] -
                          expected));
    }
  }
  EXPECT_LE(worst, 30);
}
