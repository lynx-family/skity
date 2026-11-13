// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_raster.hpp"

#include <gtest/gtest.h>

#include <cstdint>
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

TEST(SWRaster, RowClipsMatchWholeRasterAfterEdgesEndAboveThem) {
  // The oval's edges end above most clips, in the middle of rows. The circle's
  // edges round through those rows, so they must still be split there.
  skity::Path path;
  path.AddCircle(40.5f, 41.25f, 33.3f);
  path.AddOval(skity::Rect::MakeLTRB(9.25f, 20.5f, 71.75f, 58.5f));
  path.SetFillType(skity::Path::PathFillType::kEvenOdd);
  for (const int32_t rows_per_clip : {1, 7, 16, 32}) {
    ExpectClipsMatchWhole(path, skity::Matrix{}, 96, rows_per_clip);
  }
}
