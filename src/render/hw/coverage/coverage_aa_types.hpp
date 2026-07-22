// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_COVERAGE_COVERAGE_AA_TYPES_HPP
#define SRC_RENDER_HW_COVERAGE_COVERAGE_AA_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <glm/ext/vector_int2_sized.hpp>
#include <skity/geometry/vector.hpp>
#include <skity/graphic/path.hpp>

namespace skity {

constexpr int32_t kCoverageAATileWidth = 16;
constexpr int32_t kCoverageAATileHeight = 16;
constexpr int32_t kCoverageAASubpixelScale = 256;

constexpr uint32_t kInvalidCoverageAALineRangeId = 0xFFFFFFFF;

using CoverageAATileCoord = glm::i32vec2;
using CoverageAATileSize = glm::i32vec2;

struct CoverageAAGlobalLine {
  Vec2 from;
  Vec2 to;
};

struct CoverageAATileRect {
  CoverageAATileCoord origin{0};
  CoverageAATileSize size{0};

  constexpr bool Contains(CoverageAATileCoord coord) const {
    int64_t dx = coord.x - origin.x;
    int64_t dy = coord.y - origin.y;
    return dx >= 0 && dy >= 0 && dx < size.x && dy < size.y;
  }

  constexpr size_t IndexOf(CoverageAATileCoord coord) const {
    int64_t dx = coord.x - origin.x;
    int64_t dy = coord.y - origin.y;
    return dy * size.x + dx;
  }
};

struct CoverageAALineRangeId {
  uint32_t value = kInvalidCoverageAALineRangeId;

  bool IsValid() const { return value != kInvalidCoverageAALineRangeId; }
};

struct CoverageAATileLine {
  uint16_t from_x = 0;
  uint16_t from_y = 0;
  uint16_t to_x = 0;
  uint16_t to_y = 0;
  uint32_t line_range_id = kInvalidCoverageAALineRangeId;
};

struct CoverageAATile {
  int32_t tile_x = 0;
  int32_t tile_y = 0;
  CoverageAALineRangeId line_range_id;
  int32_t backdrop = 0;
};

struct CoverageAATiledPath {
  Path::PathFillType fill_type = Path::PathFillType::kWinding;
  size_t tile_offset = 0;
  size_t tile_count = 0;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_COVERAGE_COVERAGE_AA_TYPES_HPP
