// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/recorder/display_list_rtree.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using skity::DisplayListRTree;
using skity::Rect;

std::vector<int32_t> SearchOffsets(DisplayListRTree* index, const Rect& rect) {
  std::vector<int32_t> offsets;
  for (const auto& offset : index->Search(rect)) {
    offsets.push_back(offset.GetValue());
  }
  return offsets;
}

TEST(DisplayListRTree, Empty) {
  DisplayListRTree index({});

  EXPECT_TRUE(
      SearchOffsets(&index, Rect::MakeLTRB(-1e6f, -1e6f, 1e6f, 1e6f)).empty());
  EXPECT_TRUE(index
                  .SearchNonOverlappingDrawnRects(
                      Rect::MakeLTRB(-1e6f, -1e6f, 1e6f, 1e6f))
                  .empty());
}

TEST(DisplayListRTree, ManySizes) {
  constexpr int kMaxN = 32;

  std::vector<std::pair<Rect, int32_t>> spatial_ops;
  spatial_ops.reserve(kMaxN);
  for (int i = 0; i < kMaxN; ++i) {
    spatial_ops.emplace_back(Rect::MakeXYWH(i * 20, i * 20, 10, 10), i + 42);
  }

  DisplayListRTree index(std::move(spatial_ops));
  EXPECT_TRUE(SearchOffsets(&index, Rect()).empty());

  for (int i = 0; i < kMaxN; ++i) {
    const auto query = Rect::MakeXYWH(i * 20 + 2, i * 20 + 2, 6, 6);
    EXPECT_THAT(SearchOffsets(&index, query), testing::ElementsAre(i + 42));
    EXPECT_THAT(index.SearchNonOverlappingDrawnRects(query),
                testing::ElementsAre(Rect::MakeXYWH(i * 20, i * 20, 10, 10)));
  }
}

TEST(DisplayListRTree, BuildBoundarySizes) {
  constexpr int kMaxN = 65;

  std::vector<std::pair<Rect, int32_t>> all_spatial_ops;
  all_spatial_ops.reserve(kMaxN);
  for (int i = 0; i < kMaxN; ++i) {
    all_spatial_ops.emplace_back(Rect::MakeXYWH(i * 20, i * 20, 10, 10),
                                 i + 42);
  }

  for (int count : {0, 1, 7, 8, 9, 63, 64, 65}) {
    std::vector<std::pair<Rect, int32_t>> spatial_ops(
        all_spatial_ops.begin(), all_spatial_ops.begin() + count);
    DisplayListRTree index(std::move(spatial_ops));
    const auto desc = "count = " + std::to_string(count);

    EXPECT_TRUE(SearchOffsets(&index, Rect()).empty()) << desc;
    EXPECT_EQ(
        SearchOffsets(&index, Rect::MakeLTRB(-1e6f, -1e6f, 1e6f, 1e6f)).size(),
        static_cast<size_t>(count))
        << desc;

    if (count == 0) {
      EXPECT_TRUE(index
                      .SearchNonOverlappingDrawnRects(
                          Rect::MakeLTRB(-1e6f, -1e6f, 1e6f, 1e6f))
                      .empty())
          << desc;
      continue;
    }

    EXPECT_THAT(SearchOffsets(&index, Rect::MakeXYWH(2, 2, 6, 6)),
                testing::ElementsAre(42))
        << desc;
    EXPECT_THAT(
        SearchOffsets(&index, Rect::MakeXYWH((count - 1) * 20 + 2,
                                             (count - 1) * 20 + 2, 6, 6)),
        testing::ElementsAre(count + 41))
        << desc;
  }
}

TEST(DisplayListRTree, HugeSize) {
  constexpr int kN = 512;

  std::vector<std::pair<Rect, int32_t>> spatial_ops;
  spatial_ops.reserve(kN);
  for (int i = 0; i < kN; ++i) {
    spatial_ops.emplace_back(Rect::MakeXYWH(i * 20, i * 20, 10, 10), i + 42);
  }

  DisplayListRTree index(std::move(spatial_ops));
  EXPECT_TRUE(SearchOffsets(&index, Rect()).empty());

  for (int i = 0; i < kN; ++i) {
    EXPECT_THAT(
        SearchOffsets(&index, Rect::MakeXYWH(i * 20 + 2, i * 20 + 2, 6, 6)),
        testing::ElementsAre(i + 42));
  }
}

TEST(DisplayListRTree, Grid) {
  constexpr int kRows = 6;
  constexpr int kCols = 6;

  std::vector<std::pair<Rect, int32_t>> spatial_ops;
  spatial_ops.reserve(kRows * kCols);
  for (int r = 0; r < kRows; ++r) {
    for (int c = 0; c < kCols; ++c) {
      spatial_ops.emplace_back(Rect::MakeXYWH(c * 20 + 5, r * 20 + 5, 10, 10),
                               r * kCols + c + 42);
    }
  }

  DisplayListRTree index(std::move(spatial_ops));
  EXPECT_TRUE(SearchOffsets(&index, Rect()).empty());

  for (int r = 0; r < kRows; ++r) {
    for (int c = 0; c < kCols; ++c) {
      const int id = r * kCols + c + 42;
      EXPECT_THAT(
          SearchOffsets(&index, Rect::MakeXYWH(c * 20 + 7, r * 20 + 7, 6, 6)),
          testing::ElementsAre(id));
      EXPECT_THAT(
          index.SearchNonOverlappingDrawnRects(
              Rect::MakeXYWH(c * 20 + 7, r * 20 + 7, 6, 6)),
          testing::ElementsAre(Rect::MakeXYWH(c * 20 + 5, r * 20 + 5, 10, 10)));
    }
  }

  for (int r = 1; r < kRows; ++r) {
    for (int c = 1; c < kCols; ++c) {
      EXPECT_TRUE(
          SearchOffsets(&index, Rect::MakeXYWH(c * 20 - 3, r * 20 - 3, 6, 6))
              .empty());
      EXPECT_TRUE(index
                      .SearchNonOverlappingDrawnRects(
                          Rect::MakeXYWH(c * 20 - 3, r * 20 - 3, 6, 6))
                      .empty());

      EXPECT_THAT(
          SearchOffsets(&index, Rect::MakeXYWH(c * 20 - 6, r * 20 - 6, 12, 12)),
          testing::ElementsAre((r - 1) * kCols + (c - 1) + 42,
                               (r - 1) * kCols + c + 42,
                               r * kCols + (c - 1) + 42, r * kCols + c + 42));
      EXPECT_THAT(
          index.SearchNonOverlappingDrawnRects(
              Rect::MakeXYWH(c * 20 - 6, r * 20 - 6, 12, 12)),
          testing::ElementsAre(
              Rect::MakeXYWH((c - 1) * 20 + 5, (r - 1) * 20 + 5, 10, 10),
              Rect::MakeXYWH(c * 20 + 5, (r - 1) * 20 + 5, 10, 10),
              Rect::MakeXYWH((c - 1) * 20 + 5, r * 20 + 5, 10, 10),
              Rect::MakeXYWH(c * 20 + 5, r * 20 + 5, 10, 10)));
    }
  }
}

TEST(DisplayListRTree, OverlappingRects) {
  std::vector<std::pair<Rect, int32_t>> spatial_ops;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      const int x = 15 + 20 * c;
      const int y = 15 + 20 * r;
      spatial_ops.emplace_back(Rect::MakeLTRB(x - 15, y - 15, x + 15, y + 15),
                               r * 3 + c);
    }
  }

  DisplayListRTree index(std::move(spatial_ops));

  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      const int x = 15 + 20 * c;
      const int y = 15 + 20 * r;
      EXPECT_THAT(
          index.SearchNonOverlappingDrawnRects(
              Rect::MakeLTRB(x - 1, y - 1, x + 1, y + 1)),
          testing::ElementsAre(Rect::MakeLTRB(x - 15, y - 15, x + 15, y + 15)));
    }
  }

  for (int r = 0; r < 3; ++r) {
    const int x = 35;
    const int y = 15 + 20 * r;
    EXPECT_THAT(index.SearchNonOverlappingDrawnRects(
                    Rect::MakeLTRB(x - 6, y - 1, x + 6, y + 1)),
                testing::ElementsAre(Rect::MakeLTRB(0, y - 15, 70, y + 15)));
  }

  for (int c = 0; c < 3; ++c) {
    const int x = 15 + 20 * c;
    const int y = 35;
    EXPECT_THAT(index.SearchNonOverlappingDrawnRects(
                    Rect::MakeLTRB(x - 1, y - 6, x + 1, y + 6)),
                testing::ElementsAre(Rect::MakeLTRB(x - 15, 0, x + 15, 70)));
  }

  EXPECT_THAT(
      index.SearchNonOverlappingDrawnRects(Rect::MakeLTRB(29, 29, 41, 41)),
      testing::ElementsAre(Rect::MakeLTRB(0, 0, 70, 70)));
}

TEST(DisplayListRTree, Region) {
  std::vector<std::pair<Rect, int32_t>> spatial_ops;
  for (int i = 0; i < 9; ++i) {
    spatial_ops.emplace_back(Rect::MakeXYWH(i * 10, i * 10, 20, 20), i);
  }

  DisplayListRTree index(std::move(spatial_ops));
  EXPECT_THAT(
      index.SearchNonOverlappingDrawnRects(
          Rect::MakeLTRB(-100, -100, 200, 200)),
      testing::ElementsAre(
          Rect::MakeLTRB(0, 0, 20, 10), Rect::MakeLTRB(0, 10, 30, 20),
          Rect::MakeLTRB(10, 20, 40, 30), Rect::MakeLTRB(20, 30, 50, 40),
          Rect::MakeLTRB(30, 40, 60, 50), Rect::MakeLTRB(40, 50, 70, 60),
          Rect::MakeLTRB(50, 60, 80, 70), Rect::MakeLTRB(60, 70, 90, 80),
          Rect::MakeLTRB(70, 80, 100, 90), Rect::MakeLTRB(80, 90, 100, 100)));
}

}  // namespace
