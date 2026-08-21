// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_coverage_path_draw.hpp"

#include <utility>

#include "src/logging.hpp"
#include "src/render/hw/draw/geometry/wgsl_coverage_aa_tile_geometry.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"

namespace skity {

HWDynamicCoveragePathDraw::HWDynamicCoveragePathDraw(
    Matrix local_to_physical, Matrix physical_to_layer, Path path, Paint paint,
    bool enable_conflation_correction)
    : HWDynamicDraw(local_to_physical),
      physical_to_layer_(physical_to_layer),
      enable_conflation_correction_(enable_conflation_correction) {
  SetHasFragmentMask(true);
  path_groups_.emplace_back(BatchGroup<Path>{
      std::move(path),
      std::move(paint),
      std::move(local_to_physical),
  });
}

void HWDynamicCoveragePathDraw::SetTiledPathRange(size_t offset, size_t count) {
  DEBUG_CHECK(tiled_path_count_ == 0);
  tiled_path_offset_ = offset;
  tiled_path_count_ = count;
}

void HWDynamicCoveragePathDraw::SetCoverageAAFrameData(
    const CoverageAAFrameData* frame_data) {
  frame_data_ = frame_data;
}

void HWDynamicCoveragePathDraw::OnGenerateDrawStep(
    ArrayList<HWDrawStep*, 2>& steps, HWDrawContext* context) {
  if (frame_data_ == nullptr || tiled_path_count_ == 0 ||
      frame_data_->line_texture == nullptr || path_groups_.empty()) {
    return;
  }

  auto* geometry = context->arena_allocator->Make<WGSLCoverageAATileGeometry>(
      frame_data_, tiled_path_offset_, tiled_path_count_, physical_to_layer_,
      enable_conflation_correction_);
  const auto& paint = path_groups_.front().paint;
  auto* fragment = GenShadingFragment(context, paint, false);
  ConfigureShadingFragment(context, paint, GetBlendPlan(), fragment);

  steps.emplace_back(context->arena_allocator->Make<ColorStep>(
      geometry, fragment, CoverageType::kNone));
}

}  // namespace skity
