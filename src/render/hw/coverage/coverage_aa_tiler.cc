// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
//
// This file contains code derived from Pathfinder's renderer/src/tiler.rs:
// Copyright © 2020 The Pathfinder Project Developers.
// https://github.com/servo/pathfinder/blob/main/renderer/src/tiler.rs
// The derived code is used under the Apache License, Version 2.0, and has
// been modified by The Lynx Authors.

#include "src/render/hw/coverage/coverage_aa_tiler.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "src/graphic/path_visitor.hpp"

namespace skity {
namespace {

// Tile ownership is half-open, but a clipped continuous line may end exactly
// on the right or bottom boundary.
constexpr int32_t kCoverageAATileFixedLimit =
    kCoverageAATileWidth * kCoverageAASubpixelScale;

CoverageAATileCoord TileCoordForPoint(Vec2 point) {
  return {static_cast<int32_t>(
              std::floor(point.x / static_cast<float>(kCoverageAATileWidth))),
          static_cast<int32_t>(
              std::floor(point.y / static_cast<float>(kCoverageAATileHeight)))};
}

Vec2 Sample(CoverageAAGlobalLine line, float t) {
  if (t == 0.f) {
    return line.from;
  }
  if (t == 1.f) {
    return line.to;
  }
  return {line.from.x + (line.to.x - line.from.x) * t,
          line.from.y + (line.to.y - line.from.y) * t};
}

uint16_t PackFixed(float value) {
  auto fixed = static_cast<int32_t>(
      std::round(value * static_cast<float>(kCoverageAASubpixelScale)));
  return static_cast<uint16_t>(std::clamp(fixed, 0, kCoverageAATileFixedLimit));
}

}  // namespace

class CoverageAAPathTiler::PathTilingVisitor final : public PathVisitor {
 public:
  explicit PathTilingVisitor(CoverageAAPathTiler* path_tiler)
      : PathVisitor(true, Matrix{}), path_tiler_(path_tiler) {}

 private:
  void OnBeginPath() override {}
  void OnEndPath() override {}
  void OnMoveTo(Vec2 const&) override {}
  void OnLineTo(Vec2 const& p1, Vec2 const& p2) override {
    path_tiler_->ProcessGlobalLine({p1, p2});
  }
  void OnQuadTo(Vec2 const&, Vec2 const&, Vec2 const&) override {}
  void OnConicTo(Vec2 const&, Vec2 const&, Vec2 const&, float) override {}
  void OnCubicTo(Vec2 const&, Vec2 const&, Vec2 const&, Vec2 const&) override {}
  void OnClose() override {}

  CoverageAAPathTiler* path_tiler_;
};

CoverageAATiledPath CoverageAAPathTiler::Tile(const Path& path,
                                              const Matrix& local_to_global,
                                              const Rect* scissor) {
  // TODO: Transforming control points before flattening is only equivalent
  // for affine transforms. Handle perspective before Coverage AA supports it.
  auto transformed_path = path.CopyWithMatrix(local_to_global);
  auto bounds = transformed_path.GetBounds();
  if (bounds.IsEmpty()) {
    return {path.GetFillType(), tiles_.size(), 0};
  }
  if (scissor != nullptr && !bounds.Intersect(*scissor)) {
    return {path.GetFillType(), tiles_.size(), 0};
  }
  Reset(bounds);
  // Restrict only the dense tile domain. Visiting the complete contour keeps
  // edges to the left of the scissor available for row backdrop calculation.
  PathTilingVisitor visitor(this);
  visitor.VisitPath(transformed_path, true);
  return ResolveBackdrops(path.GetFillType());
}

void CoverageAAPathTiler::Reset(Rect bounds) {
  int32_t min_x =
      static_cast<int32_t>(std::floor(bounds.Left() / kCoverageAATileWidth));
  int32_t min_y =
      static_cast<int32_t>(std::floor(bounds.Top() / kCoverageAATileHeight));
  int32_t max_x =
      static_cast<int32_t>(std::ceil(bounds.Right() / kCoverageAATileWidth));
  int32_t max_y =
      static_cast<int32_t>(std::ceil(bounds.Bottom() / kCoverageAATileHeight));
  tile_bounds_ = {{min_x, min_y}, {max_x - min_x, max_y - min_y}};

  row_backdrops_.assign(tile_bounds_.size.y, 0);
  auto tile_count = static_cast<size_t>(tile_bounds_.size.x) *
                    static_cast<size_t>(tile_bounds_.size.y);
  tile_states_.assign(tile_count, {});
}

CoverageAATiledPath CoverageAAPathTiler::ResolveBackdrops(
    Path::PathFillType fill_type) {
  auto const width = tile_bounds_.size.x;
  auto const height = tile_bounds_.size.y;
  CoverageAATiledPath tiled_path;
  tiled_path.fill_type = fill_type;
  tiled_path.tile_offset = tiles_.size();

  for (int32_t y = 0; y < height; y++) {
    int32_t accumulator = row_backdrops_[y];

    for (int32_t x = 0; x < width; x++) {
      auto& tile_state = tile_states_[y * width + x];
      int32_t backdrop = accumulator + tile_state.local_backdrop;
      accumulator += tile_state.backdrop_delta;
      bool backdrop_has_coverage = fill_type == Path::PathFillType::kEvenOdd
                                       ? backdrop % 2 != 0
                                       : backdrop != 0;
      if (tile_state.line_range_id.IsValid() || backdrop_has_coverage) {
        tiles_.push_back({tile_bounds_.origin.x + x, tile_bounds_.origin.y + y,
                          tile_state.line_range_id, backdrop});
      }
    }
  }

  tiled_path.tile_count = tiles_.size() - tiled_path.tile_offset;
  return tiled_path;
}

void CoverageAAPathTiler::AddTileLine(CoverageAAGlobalLine line,
                                      CoverageAATileCoord tile_coords) {
  if (!tile_bounds_.Contains(tile_coords)) {
    return;
  }

  float tile_left = static_cast<float>(tile_coords.x) * kCoverageAATileWidth;
  float tile_top = static_cast<float>(tile_coords.y) * kCoverageAATileHeight;

  CoverageAATileLine tile_line{
      PackFixed(line.from.x - tile_left),
      PackFixed(line.from.y - tile_top),
      PackFixed(line.to.x - tile_left),
      PackFixed(line.to.y - tile_top),
  };

  if (tile_line.from_y == tile_line.to_y) {
    return;
  }

  bool is_on_left_edge = tile_line.from_x == 0 && tile_line.to_x == 0;
  bool spans_tile_height =
      std::min(tile_line.from_y, tile_line.to_y) == 0 &&
      std::max(tile_line.from_y, tile_line.to_y) == kCoverageAATileFixedLimit;
  if (is_on_left_edge && spans_tile_height) {
    // Every fragment receives +1 for a bottom-to-top full left edge, or -1
    // for the reverse direction. Folding that constant contribution into the
    // tile avoids a line-range allocation, a texture entry, and one fragment
    // shader iteration without changing coverage.
    auto& tile_state = tile_states_[tile_bounds_.IndexOf(tile_coords)];
    tile_state.local_backdrop += tile_line.from_y > tile_line.to_y ? 1 : -1;
    return;
  }

  tile_line.line_range_id = GetOrCreateLineRangeId(tile_coords).value;
  line_range_counts_[tile_line.line_range_id]++;
  lines_.push_back(tile_line);
}

void CoverageAAPathTiler::AddBackdropDelta(CoverageAATileCoord tile_coords,
                                           int32_t delta) {
  auto tile_offset_x = tile_coords.x - tile_bounds_.origin.x;
  auto tile_offset_y = tile_coords.y - tile_bounds_.origin.y;

  if (tile_offset_y < 0 || tile_offset_y >= tile_bounds_.size.y ||
      tile_offset_x >= tile_bounds_.size.x) {
    return;
  }

  if (tile_offset_x < 0) {
    row_backdrops_[static_cast<size_t>(tile_offset_y)] += delta;
    return;
  }

  auto& tile_state = tile_states_[tile_bounds_.IndexOf(tile_coords)];
  tile_state.backdrop_delta += delta;
}

CoverageAALineRangeId CoverageAAPathTiler::GetOrCreateLineRangeId(
    CoverageAATileCoord tile_coords) {
  auto& tile_state = tile_states_[tile_bounds_.IndexOf(tile_coords)];
  if (tile_state.line_range_id.IsValid()) {
    return tile_state.line_range_id;
  }

  tile_state.line_range_id.value =
      static_cast<uint32_t>(line_range_counts_.size());
  line_range_counts_.push_back(0);
  return tile_state.line_range_id;
}

void CoverageAAPathTiler::ProcessGlobalLine(CoverageAAGlobalLine line) {
  if (line.from.x == line.to.x && line.from.y == line.to.y) {
    return;
  }

  auto from_tile_coords = TileCoordForPoint(line.from);
  auto to_tile_coords = TileCoordForPoint(line.to);
  float vector_x = line.to.x - line.from.x;
  float vector_y = line.to.y - line.from.y;
  int32_t step_x = vector_x < 0.0f ? -1 : 1;
  int32_t step_y = vector_y < 0.0f ? -1 : 1;

  auto infinity = std::numeric_limits<float>::infinity();
  float first_tile_crossing_x =
      static_cast<float>(from_tile_coords.x + (vector_x >= 0.0f ? 1 : 0)) *
      kCoverageAATileWidth;
  float first_tile_crossing_y =
      static_cast<float>(from_tile_coords.y + (vector_y >= 0.0f ? 1 : 0)) *
      kCoverageAATileHeight;

  float t_max_x = vector_x == 0.0f
                      ? infinity
                      : (first_tile_crossing_x - line.from.x) / vector_x;
  float t_max_y = vector_y == 0.0f
                      ? infinity
                      : (first_tile_crossing_y - line.from.y) / vector_y;
  float t_delta_x =
      vector_x == 0.0f
          ? infinity
          : std::abs(static_cast<float>(kCoverageAATileWidth) / vector_x);
  float t_delta_y =
      vector_y == 0.0f
          ? infinity
          : std::abs(static_cast<float>(kCoverageAATileHeight) / vector_y);

  auto current_position = line.from;
  auto tile_coords = from_tile_coords;
  bool has_last_step = false;
  StepDirection last_step = StepDirection::kX;

  // Tiles own their top/left edges but not their bottom/right edges. At an
  // exact corner, positive-X lines step through X first and negative-X lines
  // step through Y first, so that ownership remains consistent in both
  // directions. Crossing a tile's left edge from left to right adds an
  // auxiliary line from the tile's bottom-left corner to the crossing;
  // crossing from right to left adds the reverse line. In y-down coordinates,
  // crossing a horizontal boundary upward adds +1 to the backdrop and
  // crossing downward adds -1. These signs must match the fragment shader's
  // edge-contribution convention.
  for (;;) {
    StepDirection next_step =
        t_max_x < t_max_y
            ? StepDirection::kX
            : (t_max_x > t_max_y
                   ? StepDirection::kY
                   : (step_x > 0 ? StepDirection::kX : StepDirection::kY));
    float next_t =
        std::min(next_step == StepDirection::kX ? t_max_x : t_max_y, 1.0f);
    bool has_next_step = tile_coords != to_tile_coords;
    auto next_position = Sample(line, next_t);
    CoverageAAGlobalLine clipped{current_position, next_position};

    AddTileLine(clipped, tile_coords);

    if (step_x < 0 && has_next_step && next_step == StepDirection::kX) {
      AddLeftBoundaryLine(tile_coords, next_position.y, false);
    } else if (step_x > 0 && has_last_step && last_step == StepDirection::kX) {
      AddLeftBoundaryLine(tile_coords, current_position.y, true);
    }

    if (step_y < 0 && has_next_step && next_step == StepDirection::kY) {
      AddBackdropDelta(tile_coords, 1);
    } else if (step_y > 0 && has_last_step && last_step == StepDirection::kY) {
      AddBackdropDelta(tile_coords, -1);
    }

    if (!has_next_step) {
      break;
    }

    if (next_step == StepDirection::kX) {
      if (tile_coords.x == to_tile_coords.x) {
        break;
      }

      t_max_x += t_delta_x;
      tile_coords.x += step_x;
    } else {
      if (tile_coords.y == to_tile_coords.y) {
        break;
      }

      t_max_y += t_delta_y;
      tile_coords.y += step_y;
    }

    current_position = next_position;
    last_step = next_step;
    has_last_step = true;
  }
}

void CoverageAAPathTiler::AddLeftBoundaryLine(CoverageAATileCoord tile_coords,
                                              float crossing_y, bool upward) {
  float left = static_cast<float>(tile_coords.x) * kCoverageAATileWidth;
  float top = static_cast<float>(tile_coords.y) * kCoverageAATileHeight;
  float bottom = top + kCoverageAATileHeight;
  float y = std::clamp(crossing_y, top, bottom);

  CoverageAAGlobalLine boundary_line =
      upward ? CoverageAAGlobalLine{{left, bottom}, {left, y}}
             : CoverageAAGlobalLine{{left, y}, {left, bottom}};
  AddTileLine(boundary_line, tile_coords);
}

}  // namespace skity
