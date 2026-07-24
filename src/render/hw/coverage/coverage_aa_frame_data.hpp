// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_COVERAGE_COVERAGE_AA_FRAME_DATA_HPP
#define SRC_RENDER_HW_COVERAGE_COVERAGE_AA_FRAME_DATA_HPP

#include <memory>
#include <vector>

#include "src/render/hw/coverage/coverage_aa_line_encoder.hpp"

namespace skity {

class GPUTexture;

struct CoverageAAFrameData {
  void Reset() {
    tiled_paths.clear();
    tiles.clear();
    encoded_lines.texture_width = 0;
    encoded_lines.texture_height = 0;
    encoded_lines.texture_data.clear();
    encoded_lines.range_offsets.clear();
    line_texture.reset();
  }

  std::vector<CoverageAATiledPath> tiled_paths;
  std::vector<CoverageAATile> tiles;
  CoverageAAEncodedLines encoded_lines;
  std::shared_ptr<GPUTexture> line_texture;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_COVERAGE_COVERAGE_AA_FRAME_DATA_HPP
