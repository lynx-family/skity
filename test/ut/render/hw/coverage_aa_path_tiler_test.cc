// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <algorithm>

#include "src/render/hw/coverage/coverage_aa_tiler.hpp"

namespace skity {

class CoverageAAPathTilerTestPeer {
 public:
  static void Reset(CoverageAAPathTiler& tiler, CoverageAATileRect bounds) {
    tiler.Reset(Rect::MakeLTRB(
        static_cast<float>(bounds.origin.x * kCoverageAATileWidth),
        static_cast<float>(bounds.origin.y * kCoverageAATileHeight),
        static_cast<float>((bounds.origin.x + bounds.size.x) *
                           kCoverageAATileWidth),
        static_cast<float>((bounds.origin.y + bounds.size.y) *
                           kCoverageAATileHeight)));
  }

  static void AddTileLine(CoverageAAPathTiler& tiler, CoverageAAGlobalLine line,
                          CoverageAATileCoord tile_coords) {
    tiler.AddTileLine(line, tile_coords);
  }

  static void AddBackdropDelta(CoverageAAPathTiler& tiler,
                               CoverageAATileCoord tile_coords, int32_t delta) {
    tiler.AddBackdropDelta(tile_coords, delta);
  }

  static void ProcessGlobalLine(CoverageAAPathTiler& tiler,
                                CoverageAAGlobalLine line) {
    tiler.ProcessGlobalLine(line);
  }

  static CoverageAATiledPath ResolveBackdrops(
      CoverageAAPathTiler& tiler,
      Path::PathFillType fill_type = Path::PathFillType::kWinding) {
    return tiler.ResolveBackdrops(fill_type);
  }

  static const auto& TileState(const CoverageAAPathTiler& tiler,
                               CoverageAATileCoord coord) {
    return tiler.tile_states_[tiler.tile_bounds_.IndexOf(coord)];
  }

  static const std::vector<int32_t>& RowBackdrops(
      const CoverageAAPathTiler& tiler) {
    return tiler.row_backdrops_;
  }

  static const CoverageAATileRect& TileBounds(
      const CoverageAAPathTiler& tiler) {
    return tiler.tile_bounds_;
  }
};

}  // namespace skity

namespace {

class TestTiler {
 public:
  TestTiler() : tiler(tiles, lines, line_range_counts) {}

  explicit TestTiler(skity::CoverageAATileRect tile_bounds) : TestTiler() {
    skity::CoverageAAPathTilerTestPeer::Reset(tiler, tile_bounds);
  }

  skity::CoverageAATiledPath Tile(
      const skity::Path& path,
      const skity::Matrix& local_to_global = skity::Matrix{},
      const skity::Rect* scissor = nullptr) {
    return tiler.Tile(path, local_to_global, scissor);
  }

  std::vector<skity::CoverageAATile> tiles;
  std::vector<skity::CoverageAATileLine> lines;
  std::vector<uint32_t> line_range_counts;
  skity::CoverageAAPathTiler tiler;
};

skity::CoverageAAGlobalLine Line(float from_x, float from_y, float to_x,
                                 float to_y) {
  return {{from_x, from_y}, {to_x, to_y}};
}

const auto& TileState(const TestTiler& tiler,
                      skity::CoverageAATileCoord coord) {
  return skity::CoverageAAPathTilerTestPeer::TileState(tiler.tiler, coord);
}

const skity::CoverageAATile* FindTile(
    const TestTiler& tiler, const skity::CoverageAATiledPath& tiled_path,
    skity::CoverageAATileCoord coord) {
  auto begin = tiler.tiles.begin() + tiled_path.tile_offset;
  auto end = begin + tiled_path.tile_count;
  auto it =
      std::find_if(begin, end, [coord](const skity::CoverageAATile& tile) {
        return tile.tile_x == coord.x && tile.tile_y == coord.y;
      });
  return it == end ? nullptr : &*it;
}

const std::vector<skity::CoverageAATileLine>& Lines(const TestTiler& tiler) {
  return tiler.lines;
}

const std::vector<int32_t>& RowBackdrops(const TestTiler& tiler) {
  return skity::CoverageAAPathTilerTestPeer::RowBackdrops(tiler.tiler);
}

const skity::CoverageAATileRect& TileBounds(const TestTiler& tiler) {
  return skity::CoverageAAPathTilerTestPeer::TileBounds(tiler.tiler);
}

void AddTileLine(TestTiler& tiler, skity::CoverageAAGlobalLine line,
                 skity::CoverageAATileCoord tile_coords) {
  skity::CoverageAAPathTilerTestPeer::AddTileLine(tiler.tiler, line,
                                                  tile_coords);
}

void AddBackdropDelta(TestTiler& tiler, skity::CoverageAATileCoord tile_coords,
                      int32_t delta) {
  skity::CoverageAAPathTilerTestPeer::AddBackdropDelta(tiler.tiler, tile_coords,
                                                       delta);
}

void ProcessGlobalLine(TestTiler& tiler, skity::CoverageAAGlobalLine line) {
  skity::CoverageAAPathTilerTestPeer::ProcessGlobalLine(tiler.tiler, line);
}

skity::CoverageAATiledPath ResolveBackdrops(
    TestTiler& tiler,
    skity::Path::PathFillType fill_type = skity::Path::PathFillType::kWinding) {
  return skity::CoverageAAPathTilerTestPeer::ResolveBackdrops(tiler.tiler,
                                                              fill_type);
}

skity::Path MakeCoverageAASquareCW() {
  skity::Path path;
  path.MoveTo(16, 16);
  path.LineTo(48, 16);
  path.LineTo(48, 48);
  path.LineTo(16, 48);
  path.Close();
  return path;
}

skity::Path MakeCoverageAAInsetSquareCW() {
  skity::Path path;
  path.MoveTo(20, 20);
  path.LineTo(44, 20);
  path.LineTo(44, 44);
  path.LineTo(20, 44);
  path.Close();
  return path;
}

skity::Path MakeCoverageAALocalSquareCW() {
  skity::Path path;
  path.MoveTo(0, 0);
  path.LineTo(32, 0);
  path.LineTo(32, 32);
  path.LineTo(0, 32);
  path.Close();
  return path;
}

}  // namespace

TEST(CoverageAAPathTiler, TileRect_Contains_HalfOpen) {
  skity::CoverageAATileRect rect{{2, 3}, {4, 5}};

  EXPECT_TRUE(rect.Contains({2, 3}));
  EXPECT_TRUE(rect.Contains({5, 7}));

  EXPECT_FALSE(rect.Contains({1, 3}));
  EXPECT_FALSE(rect.Contains({2, 2}));
  EXPECT_FALSE(rect.Contains({6, 3}));
  EXPECT_FALSE(rect.Contains({2, 8}));
}

TEST(CoverageAAPathTiler, TileRect_Contains_EmptyRect) {
  skity::CoverageAATileRect empty{{2, 3}, {0, 5}};

  EXPECT_FALSE(empty.Contains({2, 3}));
}

TEST(CoverageAAPathTiler, AddTileLine_FoldsFullLeftEdgeIntoLocalBackdrop) {
  TestTiler tiler({{0, 0}, {1, 1}});

  AddTileLine(tiler, Line(0, 0, 0, 16), {0, 0});

  EXPECT_TRUE(Lines(tiler).empty());
  EXPECT_FALSE(TileState(tiler, {0, 0}).line_range_id.IsValid());
  EXPECT_EQ(TileState(tiler, {0, 0}).local_backdrop, -1);

  AddTileLine(tiler, Line(0, 16, 0, 0), {0, 0});

  EXPECT_TRUE(Lines(tiler).empty());
  EXPECT_EQ(TileState(tiler, {0, 0}).local_backdrop, 0);
}

TEST(CoverageAAPathTiler, AddTileLine_CullsHorizontalLine) {
  TestTiler tiler({{0, 0}, {1, 1}});

  AddTileLine(tiler, Line(0, 0, 16, 0), {0, 0});

  EXPECT_TRUE(Lines(tiler).empty());
  EXPECT_FALSE(TileState(tiler, {0, 0}).line_range_id.IsValid());
}

TEST(CoverageAAPathTiler, AddTileLine_CullsOutOfBoundsTile) {
  TestTiler tiler({{0, 0}, {1, 1}});

  AddTileLine(tiler, Line(16, 0, 16, 16), {1, 0});

  EXPECT_TRUE(Lines(tiler).empty());
}

TEST(CoverageAAPathTiler, AddTileLine_EncodesTileBoundaryAs4096) {
  TestTiler tiler({{0, 0}, {1, 1}});

  AddTileLine(tiler, Line(0, 16, 16, 0), {0, 0});

  ASSERT_EQ(Lines(tiler).size(), 1u);
  EXPECT_EQ(Lines(tiler)[0].from_x, 0);
  EXPECT_EQ(Lines(tiler)[0].from_y, 4096);
  EXPECT_EQ(Lines(tiler)[0].to_x, 4096);
  EXPECT_EQ(Lines(tiler)[0].to_y, 0);
}

TEST(CoverageAAPathTiler, AddTileLine_RoundsToNearestSubpixel) {
  TestTiler tiler({{0, 0}, {1, 1}});
  constexpr float scale = skity::kCoverageAASubpixelScale;

  AddTileLine(tiler, Line(324.999f / scale, 4095.996f / scale, 8.0f, 0.0f),
              {0, 0});

  ASSERT_EQ(Lines(tiler).size(), 1u);
  EXPECT_EQ(Lines(tiler)[0].from_x, 325);
  EXPECT_EQ(Lines(tiler)[0].from_y, 4096);
}

TEST(CoverageAAPathTiler, AddTileLine_ReusesLineRange) {
  TestTiler tiler({{0, 0}, {1, 1}});

  AddTileLine(tiler, Line(8, 0, 8, 16), {0, 0});
  AddTileLine(tiler, Line(12, 0, 12, 16), {0, 0});

  ASSERT_EQ(Lines(tiler).size(), 2u);
  EXPECT_EQ(Lines(tiler)[0].line_range_id, Lines(tiler)[1].line_range_id);
  EXPECT_EQ(Lines(tiler)[0].line_range_id, 0u);
  EXPECT_EQ(tiler.line_range_counts, (std::vector<uint32_t>{2}));
}

TEST(CoverageAAPathTiler, AddBackdropDelta_InsideTile) {
  TestTiler tiler({{1, 2}, {2, 2}});

  AddBackdropDelta(tiler, {2, 3}, 1);
  AddBackdropDelta(tiler, {2, 3}, 1);

  EXPECT_EQ(TileState(tiler, {2, 3}).backdrop_delta, 2);
}

TEST(CoverageAAPathTiler, AddBackdropDelta_PreservesDeepWinding) {
  TestTiler tiler({{0, 0}, {1, 1}});

  for (int i = 0; i < 256; i++) {
    AddBackdropDelta(tiler, {0, 0}, 1);
  }

  EXPECT_EQ(TileState(tiler, {0, 0}).backdrop_delta, 256);
}

TEST(CoverageAAPathTiler, AddBackdropDelta_LeftOfBoundsAccumulatesRowBackdrop) {
  TestTiler tiler({{1, 2}, {2, 2}});

  AddBackdropDelta(tiler, {0, 3}, 1);
  AddBackdropDelta(tiler, {0, 3}, -2);

  ASSERT_EQ(RowBackdrops(tiler).size(), 2u);
  EXPECT_EQ(RowBackdrops(tiler)[0], 0);
  EXPECT_EQ(RowBackdrops(tiler)[1], -1);
}

TEST(CoverageAAPathTiler, AddBackdropDelta_OutsideRowsIsIgnored) {
  TestTiler tiler({{1, 2}, {2, 2}});

  AddBackdropDelta(tiler, {0, 1}, 1);
  AddBackdropDelta(tiler, {0, 4}, 1);

  EXPECT_EQ(RowBackdrops(tiler), (std::vector<int32_t>{0, 0}));
}

TEST(CoverageAAPathTiler, ProcessGlobalLine_SingleTile) {
  TestTiler tiler({{0, 0}, {1, 1}});

  ProcessGlobalLine(tiler, Line(4, 4, 8, 12));

  ASSERT_EQ(Lines(tiler).size(), 1u);
  EXPECT_EQ(Lines(tiler)[0].from_x, 1024);
  EXPECT_EQ(Lines(tiler)[0].from_y, 1024);
  EXPECT_EQ(Lines(tiler)[0].to_x, 2048);
  EXPECT_EQ(Lines(tiler)[0].to_y, 3072);
}

TEST(CoverageAAPathTiler, ProcessGlobalLine_VerticalMultiTile) {
  TestTiler tiler({{0, 0}, {1, 3}});

  ProcessGlobalLine(tiler, Line(8, 8, 8, 40));

  ASSERT_EQ(Lines(tiler).size(), 3u);
  EXPECT_EQ(Lines(tiler)[0].from_y, 2048);
  EXPECT_EQ(Lines(tiler)[0].to_y, 4096);
  EXPECT_EQ(Lines(tiler)[1].from_y, 0);
  EXPECT_EQ(Lines(tiler)[1].to_y, 4096);
  EXPECT_EQ(Lines(tiler)[2].from_y, 0);
  EXPECT_EQ(Lines(tiler)[2].to_y, 2048);
}

TEST(CoverageAAPathTiler, ProcessGlobalLine_FoldsFullLeftEdge) {
  TestTiler tiler({{1, 1}, {1, 1}});

  ProcessGlobalLine(tiler, Line(16, 16, 16, 32));

  EXPECT_TRUE(Lines(tiler).empty());
  EXPECT_EQ(TileState(tiler, {1, 1}).backdrop_delta, 0);
  EXPECT_EQ(TileState(tiler, {1, 1}).local_backdrop, -1);
}

TEST(CoverageAAPathTiler, Tiling_FoldsFullLeftBoundaryLineOnRightTile) {
  TestTiler tiler({{1, 1}, {2, 1}});

  ProcessGlobalLine(tiler, Line(16, 16, 48, 16));

  EXPECT_TRUE(Lines(tiler).empty());
  EXPECT_EQ(TileState(tiler, {1, 1}).local_backdrop, 0);
  EXPECT_EQ(TileState(tiler, {2, 1}).local_backdrop, 1);
}

TEST(CoverageAAPathTiler, Tiling_LeftBoundaryLineStartsAtCrossingY) {
  TestTiler tiler({{1, 1}, {1, 1}});

  ProcessGlobalLine(tiler, Line(8, 20, 20, 28));

  ASSERT_EQ(Lines(tiler).size(), 2u);
  EXPECT_EQ(Lines(tiler)[1].from_x, 0);
  EXPECT_EQ(Lines(tiler)[1].from_y, 4096);
  EXPECT_EQ(Lines(tiler)[1].to_x, 0);
  EXPECT_EQ(Lines(tiler)[1].to_y, 2389);
}

TEST(CoverageAAPathTiler,
     ProcessGlobalLine_RightToLeftAddsReversedBoundaryLine) {
  TestTiler tiler({{0, 0}, {2, 1}});

  ProcessGlobalLine(tiler, Line(24, 4, 8, 12));

  ASSERT_EQ(Lines(tiler).size(), 3u);
  EXPECT_EQ(Lines(tiler)[1].from_x, 0);
  EXPECT_EQ(Lines(tiler)[1].from_y, 2048);
  EXPECT_EQ(Lines(tiler)[1].to_x, 0);
  EXPECT_EQ(Lines(tiler)[1].to_y, 4096);
}

TEST(CoverageAAPathTiler, ProcessGlobalLine_PreservesExactEndpoint) {
  TestTiler tiler({{-7, 0}, {11, 1}});

  // Recomputing the endpoint as from + (to - from) * 1 loses enough precision
  // for its fixed-point x coordinate to round to 3294 instead of 3295.
  ProcessGlobalLine(tiler, Line(61.7542038f, 1.0f, -99.1308594f, 2.0f));

  ASSERT_FALSE(Lines(tiler).empty());
  EXPECT_EQ(Lines(tiler).back().to_x, 3295);
}

TEST(CoverageAAPathTiler, Tiling_LeftEdgeCreatesBackdropDeltaAtTopCrossing) {
  TestTiler tiler({{1, 1}, {1, 2}});

  ProcessGlobalLine(tiler, Line(16, 48, 16, 16));

  EXPECT_EQ(TileState(tiler, {1, 1}).backdrop_delta, 0);
  EXPECT_EQ(TileState(tiler, {1, 2}).backdrop_delta, 1);
}

TEST(CoverageAAPathTiler, Tiling_RectCCW_BackdropSignIsOpposite) {
  TestTiler tiler({{1, 1}, {2, 2}});

  ProcessGlobalLine(tiler, Line(16, 16, 16, 48));
  ProcessGlobalLine(tiler, Line(16, 48, 48, 48));
  ProcessGlobalLine(tiler, Line(48, 48, 48, 16));
  ProcessGlobalLine(tiler, Line(48, 16, 16, 16));

  EXPECT_EQ(TileState(tiler, {1, 2}).backdrop_delta, -1);
}

TEST(CoverageAAPathTiler, ResolveBackdrops_PropagatesLeftToRight) {
  TestTiler tiler({{1, 2}, {2, 1}});

  AddBackdropDelta(tiler, {1, 2}, 1);
  auto tiled_path = ResolveBackdrops(tiler);

  EXPECT_EQ(FindTile(tiler, tiled_path, {1, 2}), nullptr);
  ASSERT_NE(FindTile(tiler, tiled_path, {2, 2}), nullptr);
  EXPECT_EQ(FindTile(tiler, tiled_path, {2, 2})->backdrop, 1);
}

TEST(CoverageAAPathTiler, ResolveBackdrops_DoesNotAllocateLineRange) {
  TestTiler tiler({{1, 2}, {2, 1}});

  AddBackdropDelta(tiler, {1, 2}, 1);
  auto tiled_path = ResolveBackdrops(tiler);

  ASSERT_EQ(tiled_path.tile_count, 1u);
  EXPECT_FALSE(tiler.tiles[tiled_path.tile_offset].line_range_id.IsValid());
}

TEST(CoverageAAPathTiler, ResolveBackdrops_CullsEvenOddSolidTile) {
  TestTiler tiler({{0, 0}, {3, 1}});

  AddBackdropDelta(tiler, {0, 0}, 2);
  AddBackdropDelta(tiler, {1, 0}, -1);
  auto tiled_path =
      ResolveBackdrops(tiler, skity::Path::PathFillType::kEvenOdd);

  ASSERT_EQ(tiled_path.tile_count, 1u);
  EXPECT_EQ(FindTile(tiler, tiled_path, {1, 0}), nullptr);
  ASSERT_NE(FindTile(tiler, tiled_path, {2, 0}), nullptr);
  EXPECT_EQ(FindTile(tiler, tiled_path, {2, 0})->backdrop, 1);
}

TEST(CoverageAAPathTiler, ResolveBackdrops_PreservesBackdropDelta) {
  TestTiler tiler({{1, 2}, {2, 1}});

  AddBackdropDelta(tiler, {1, 2}, 1);
  EXPECT_EQ(TileState(tiler, {1, 2}).backdrop_delta, 1);

  auto tiled_path = ResolveBackdrops(tiler);

  EXPECT_EQ(TileState(tiler, {1, 2}).backdrop_delta, 1);
  ASSERT_NE(FindTile(tiler, tiled_path, {2, 2}), nullptr);
  EXPECT_EQ(FindTile(tiler, tiled_path, {2, 2})->backdrop, 1);
}

TEST(CoverageAAPathTiler, Tile_FoldsTileAlignedRectLines) {
  TestTiler tiler;

  tiler.Tile(MakeCoverageAASquareCW());

  EXPECT_EQ(TileBounds(tiler).origin, (skity::CoverageAATileCoord{1, 1}));
  EXPECT_EQ(TileBounds(tiler).size.x, 2);
  EXPECT_EQ(TileBounds(tiler).size.y, 2);
  EXPECT_TRUE(tiler.lines.empty());
}

TEST(CoverageAAPathTiler, Tile_AppliesMatrixBeforeTiling) {
  TestTiler tiler;

  tiler.Tile(MakeCoverageAALocalSquareCW(), skity::Matrix::Translate(16, 16));

  EXPECT_EQ(TileBounds(tiler).origin, (skity::CoverageAATileCoord{1, 1}));
  EXPECT_EQ(TileBounds(tiler).size.x, 2);
  EXPECT_EQ(TileBounds(tiler).size.y, 2);
  EXPECT_TRUE(tiler.lines.empty());
}

TEST(CoverageAAPathTiler, Tile_UsesTransformedPathBounds) {
  skity::Path path;
  path.MoveTo(0, 0);
  path.LineTo(32, 0);
  path.LineTo(0, 32);
  path.Close();
  TestTiler tiler;

  tiler.Tile(path, skity::Matrix::RotateDeg(45, {0, 0}));

  EXPECT_EQ(TileBounds(tiler).origin, (skity::CoverageAATileCoord{-2, 0}));
  EXPECT_EQ(TileBounds(tiler).size, (skity::CoverageAATileSize{4, 2}));
}

TEST(CoverageAAPathTiler, Tile_ScissorLimitsBoundsAndPreservesBackdrop) {
  skity::Path path;
  path.MoveTo(-160, -160);
  path.LineTo(160, -160);
  path.LineTo(160, 160);
  path.LineTo(-160, 160);
  path.Close();
  const skity::Rect scissor = skity::Rect::MakeLTRB(17, 17, 31, 31);
  TestTiler tiler;

  auto tiled_path = tiler.Tile(path, skity::Matrix{}, &scissor);

  EXPECT_EQ(TileBounds(tiler).origin, (skity::CoverageAATileCoord{1, 1}));
  EXPECT_EQ(TileBounds(tiler).size, (skity::CoverageAATileSize{1, 1}));
  ASSERT_EQ(tiled_path.tile_count, 1u);
  auto tile = FindTile(tiler, tiled_path, {1, 1});
  ASSERT_NE(tile, nullptr);
  EXPECT_EQ(tile->backdrop, 1);
  EXPECT_FALSE(tile->line_range_id.IsValid());
  EXPECT_TRUE(tiler.lines.empty());
}

TEST(CoverageAAPathTiler, Tile_EmptyPathDoesNotAppendFrameData) {
  TestTiler tiler;
  tiler.Tile(MakeCoverageAAInsetSquareCW());
  size_t tile_count = tiler.tiles.size();
  size_t line_count = tiler.lines.size();
  size_t line_range_count = tiler.line_range_counts.size();
  skity::Path empty_path;
  empty_path.SetFillType(skity::Path::PathFillType::kEvenOdd);

  auto tiled_path = tiler.Tile(empty_path);

  EXPECT_EQ(tiled_path.fill_type, skity::Path::PathFillType::kEvenOdd);
  EXPECT_EQ(tiled_path.tile_offset, tile_count);
  EXPECT_EQ(tiled_path.tile_count, 0u);
  EXPECT_EQ(tiler.tiles.size(), tile_count);
  EXPECT_EQ(tiler.lines.size(), line_count);
  EXPECT_EQ(tiler.line_range_counts.size(), line_range_count);
}

TEST(CoverageAAPathTiler, Tile_FullyClippedPathDoesNotAppendFrameData) {
  TestTiler tiler;
  auto path = MakeCoverageAAInsetSquareCW();
  tiler.Tile(path);
  size_t tile_count = tiler.tiles.size();
  size_t line_count = tiler.lines.size();
  size_t line_range_count = tiler.line_range_counts.size();
  const skity::Rect scissor = skity::Rect::MakeLTRB(100, 100, 120, 120);

  auto tiled_path = tiler.Tile(path, skity::Matrix{}, &scissor);

  EXPECT_EQ(tiled_path.tile_offset, tile_count);
  EXPECT_EQ(tiled_path.tile_count, 0u);
  EXPECT_EQ(tiler.tiles.size(), tile_count);
  EXPECT_EQ(tiler.lines.size(), line_count);
  EXPECT_EQ(tiler.line_range_counts.size(), line_range_count);
}

TEST(CoverageAAPathTiler, Tile_RectCWMatchesExpectedBackdrop) {
  TestTiler tiler;

  auto tiled_path = tiler.Tile(MakeCoverageAASquareCW());

  ASSERT_NE(FindTile(tiler, tiled_path, {1, 1}), nullptr);
  ASSERT_NE(FindTile(tiler, tiled_path, {2, 1}), nullptr);
  ASSERT_NE(FindTile(tiler, tiled_path, {1, 2}), nullptr);
  ASSERT_NE(FindTile(tiler, tiled_path, {2, 2}), nullptr);
  EXPECT_EQ(FindTile(tiler, tiled_path, {1, 1})->backdrop, 1);
  EXPECT_EQ(FindTile(tiler, tiled_path, {2, 1})->backdrop, 1);
  EXPECT_EQ(FindTile(tiler, tiled_path, {1, 2})->backdrop, 1);
  EXPECT_EQ(FindTile(tiler, tiled_path, {2, 2})->backdrop, 1);
  EXPECT_FALSE(FindTile(tiler, tiled_path, {1, 1})->line_range_id.IsValid());
  EXPECT_FALSE(FindTile(tiler, tiled_path, {2, 1})->line_range_id.IsValid());
  EXPECT_FALSE(FindTile(tiler, tiled_path, {1, 2})->line_range_id.IsValid());
  EXPECT_FALSE(FindTile(tiler, tiled_path, {2, 2})->line_range_id.IsValid());
}

TEST(CoverageAAPathTiler, TiledLines_ReferenceLineRange) {
  TestTiler tiler;

  tiler.Tile(MakeCoverageAAInsetSquareCW());

  ASSERT_FALSE(tiler.lines.empty());
  EXPECT_FALSE(tiler.line_range_counts.empty());
  for (auto const& line : tiler.lines) {
    EXPECT_NE(line.line_range_id, skity::kInvalidCoverageAALineRangeId);
    EXPECT_LT(line.line_range_id, tiler.line_range_counts.size());
  }
}

TEST(CoverageAAPathTiler, Tile_AppendsFrameLinesAndRanges) {
  TestTiler tiler;

  auto first_path = tiler.Tile(MakeCoverageAAInsetSquareCW());
  size_t first_line_count = tiler.lines.size();
  size_t first_line_range_count = tiler.line_range_counts.size();
  auto second_path = tiler.Tile(MakeCoverageAAInsetSquareCW());

  EXPECT_EQ(first_path.tile_offset, 0u);
  EXPECT_EQ(first_path.tile_count, 4u);
  EXPECT_EQ(second_path.tile_offset, 4u);
  EXPECT_EQ(second_path.tile_count, 4u);
  ASSERT_EQ(tiler.tiles.size(), 8u);
  ASSERT_GT(first_line_count, 0u);
  ASSERT_GT(first_line_range_count, 0u);
  ASSERT_EQ(tiler.lines.size(), first_line_count * 2);
  ASSERT_EQ(tiler.line_range_counts.size(), first_line_range_count * 2);
  size_t second_path_end = second_path.tile_offset + second_path.tile_count;
  for (size_t i = second_path.tile_offset; i < second_path_end; ++i) {
    const auto& tile = tiler.tiles[i];
    if (tile.line_range_id.IsValid()) {
      EXPECT_GE(tile.line_range_id.value, first_line_range_count);
    }
  }
}

TEST(CoverageAAPathTiler, TiledPath_SolidBackdropTileHasNoLines) {
  TestTiler tiler;

  auto tiled_path = tiler.Tile(MakeCoverageAASquareCW());

  auto solid_tile = FindTile(tiler, tiled_path, {2, 2});

  ASSERT_NE(solid_tile, nullptr);
  EXPECT_FALSE(solid_tile->line_range_id.IsValid());
  EXPECT_EQ(solid_tile->backdrop, 1);
}

TEST(CoverageAAPathTiler, TiledPath_EmitsBoundaryTile) {
  TestTiler tiler;

  auto tiled_path = tiler.Tile(MakeCoverageAAInsetSquareCW());

  ASSERT_EQ(tiled_path.tile_count, 4u);

  auto boundary_tile = FindTile(tiler, tiled_path, {1, 1});

  ASSERT_NE(boundary_tile, nullptr);
  EXPECT_TRUE(boundary_tile->line_range_id.IsValid());
  EXPECT_LT(boundary_tile->line_range_id.value, tiler.line_range_counts.size());
  EXPECT_EQ(boundary_tile->backdrop, 0);
}
