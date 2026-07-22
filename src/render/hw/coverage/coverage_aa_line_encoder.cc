// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/coverage/coverage_aa_line_encoder.hpp"

#include <algorithm>
#include <cstddef>

#include "src/geometry/math.hpp"
#include "src/logging.hpp"

namespace skity {
namespace {

constexpr size_t kCoverageAAComponentsPerTexel = 4;
constexpr size_t kCoverageAAMinTextureWidth = 16;
constexpr size_t kCoverageAAMaxTextureWidth = 4096;

size_t GetTextureWidth(size_t texel_count, uint32_t max_texture_size) {
  auto max_texture_width =
      std::min<size_t>(max_texture_size, kCoverageAAMaxTextureWidth);
  size_t result = std::min<size_t>(
      std::max<size_t>(NextPow2(texel_count), kCoverageAAMinTextureWidth),
      max_texture_width);
  return result;
}

constexpr void WriteLine(const CoverageAATileLine& line,
                         uint16_t* destination) {
  destination[0] = line.from_x;
  destination[1] = line.from_y;
  destination[2] = line.to_x;
  destination[3] = line.to_y;
}

}  // namespace

void EncodeCoverageAALines(const std::vector<CoverageAATileLine>& lines,
                           std::vector<uint32_t>& line_range_counts,
                           uint32_t max_texture_size,
                           CoverageAAEncodedLines& encoded_lines) {
  DEBUG_CHECK(max_texture_size > 0);

  encoded_lines.texture_data.clear();

  size_t texel_count = std::max<size_t>(1, lines.size());
  size_t texture_width = GetTextureWidth(texel_count, max_texture_size);
  size_t texture_height = (texel_count + texture_width - 1) / texture_width;

  encoded_lines.range_offsets.resize(line_range_counts.size() + 1);
  uint32_t line_cursor = 0;
  for (size_t i = 0; i < line_range_counts.size(); ++i) {
    uint32_t line_count = line_range_counts[i];
    encoded_lines.range_offsets[i] = line_cursor;
    line_range_counts[i] = line_cursor;
    line_cursor += line_count;
  }
  encoded_lines.range_offsets.back() = line_cursor;

  encoded_lines.texture_width = static_cast<uint32_t>(texture_width);
  encoded_lines.texture_height = static_cast<uint32_t>(texture_height);
  encoded_lines.texture_data.resize(texture_width * texture_height *
                                    kCoverageAAComponentsPerTexel);

  // Counts are no longer needed after building offsets, so reuse them as write
  // cursors while grouping lines into their tile's contiguous range.
  for (auto const& line : lines) {
    uint32_t line_index = line_range_counts[line.line_range_id]++;
    WriteLine(line, encoded_lines.texture_data.data() +
                        static_cast<size_t>(line_index) *
                            kCoverageAAComponentsPerTexel);
  }
  line_range_counts.clear();
}

}  // namespace skity
