// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_COVERAGE_COVERAGE_AA_RENDERER_HPP
#define SRC_RENDER_HW_COVERAGE_COVERAGE_AA_RENDERER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "src/base/base_macros.hpp"
#include "src/render/hw/coverage/coverage_aa_frame_data.hpp"
#include "src/render/hw/coverage/coverage_aa_tiler.hpp"

namespace skity {

class GPUCommandBuffer;
class HWDynamicCoveragePathDraw;
struct HWDrawContext;

class CoverageAARenderer final {
 public:
  CoverageAARenderer();

  void BeginFrame();

  void AddDraw(HWDynamicCoveragePathDraw* draw);

  void PrepareFrame(HWDrawContext* context, GPUCommandBuffer* command_buffer);

 private:
  SKITY_DISALLOW_COPY_ASSIGN_AND_MOVE(CoverageAARenderer);

  std::vector<HWDynamicCoveragePathDraw*> draws_;
  // Keep this in sync when batching adds Paths to a registered draw.
  size_t tiled_path_count_ = 0;
  CoverageAAFrameData frame_data_;
  std::vector<CoverageAATileLine> lines_;
  std::vector<uint32_t> line_range_counts_;
  std::unique_ptr<CoverageAAPathTiler> path_tiler_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_COVERAGE_COVERAGE_AA_RENDERER_HPP
