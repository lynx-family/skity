// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_COVERAGE_COVERAGE_AA_TILER_HPP
#define SRC_RENDER_HW_COVERAGE_COVERAGE_AA_TILER_HPP

#include <skity/geometry/matrix.hpp>
#include <vector>

#include "src/render/hw/coverage/coverage_aa_types.hpp"

namespace skity {

class CoverageAAPathTilerTestPeer;

class CoverageAAPathTiler {
 public:
  CoverageAAPathTiler(std::vector<CoverageAATile>& tiles,
                      std::vector<CoverageAATileLine>& lines,
                      std::vector<uint32_t>& line_range_counts)
      : tiles_(tiles), lines_(lines), line_range_counts_(line_range_counts) {}

  CoverageAATiledPath Tile(const Path& path,
                           const Matrix& local_to_global = Matrix{},
                           const Rect* scissor = nullptr);

 private:
  class PathTilingVisitor;
  struct TileState {
    CoverageAALineRangeId line_range_id;
    // Applied after this tile while resolving a row, so it propagates to all
    // tiles on the right.
    int16_t backdrop_delta = 0;
    // A full-height line on the tile's left edge contributes the same signed
    // winding to every fragment in this tile. As an optimization, that line
    // is folded into local_backdrop instead of being stored in the line
    // texture. Unlike backdrop_delta, this affects only the current tile.
    int16_t local_backdrop = 0;
  };
  static_assert(sizeof(TileState) == 8);
  friend class CoverageAAPathTilerTestPeer;

  void Reset(Rect bounds);
  void ProcessGlobalLine(CoverageAAGlobalLine line);
  CoverageAATiledPath ResolveBackdrops(Path::PathFillType fill_type);

  void AddTileLine(CoverageAAGlobalLine line, CoverageAATileCoord tile_coords);
  void AddBackdropDelta(CoverageAATileCoord tile_coords, int32_t delta);

  CoverageAATileRect tile_bounds_;
  std::vector<int32_t> row_backdrops_;
  std::vector<TileState> tile_states_;
  std::vector<CoverageAATile>& tiles_;
  std::vector<CoverageAATileLine>& lines_;
  std::vector<uint32_t>& line_range_counts_;

  enum class StepDirection {
    kX,
    kY,
  };

  void AddLeftBoundaryLine(CoverageAATileCoord tile_coords, float crossing_y,
                           bool upward);
  CoverageAALineRangeId GetOrCreateLineRangeId(CoverageAATileCoord tile_coords);
};

}  // namespace skity

#endif  // SRC_RENDER_HW_COVERAGE_COVERAGE_AA_TILER_HPP
