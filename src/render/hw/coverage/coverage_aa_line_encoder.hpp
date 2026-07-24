// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_COVERAGE_COVERAGE_AA_LINE_ENCODER_HPP
#define SRC_RENDER_HW_COVERAGE_COVERAGE_AA_LINE_ENCODER_HPP

#include <cstdint>
#include <vector>

#include "src/render/hw/coverage/coverage_aa_types.hpp"

namespace skity {

// GPU wire format: each tile-local coordinate is unsigned 8.8 fixed point in
// [0, 16], encoded as [0, 4096]. One line occupies one RGBA16Uint texel storing
// from_x/from_y/to_x/to_y. The shader divides the loaded values by
// kCoverageAASubpixelScale.
struct CoverageAAEncodedLines {
  uint32_t texture_width = 0;
  uint32_t texture_height = 0;
  std::vector<uint16_t> texture_data;
  std::vector<uint32_t> range_offsets;
};

void EncodeCoverageAALines(const std::vector<CoverageAATileLine>& lines,
                           std::vector<uint32_t>& line_range_counts,
                           uint32_t max_texture_size,
                           CoverageAAEncodedLines& encoded_lines);

}  // namespace skity

#endif  // SRC_RENDER_HW_COVERAGE_COVERAGE_AA_LINE_ENCODER_HPP
