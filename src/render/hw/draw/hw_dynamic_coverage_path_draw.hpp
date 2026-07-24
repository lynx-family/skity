// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_DYNAMIC_COVERAGE_PATH_DRAW_HPP
#define SRC_RENDER_HW_DRAW_HW_DYNAMIC_COVERAGE_PATH_DRAW_HPP

#include <cstddef>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <utility>
#include <vector>

#include "src/render/hw/coverage/coverage_aa_frame_data.hpp"
#include "src/render/hw/draw/hw_dynamic_draw.hpp"
#include "src/utils/batch_group.hpp"

namespace skity {

class HWDynamicCoveragePathDraw : public HWDynamicDraw {
 public:
  HWDynamicCoveragePathDraw(Matrix local_to_physical, Matrix physical_to_layer,
                            Path path, Paint paint);

  ~HWDynamicCoveragePathDraw() override = default;

  const std::vector<BatchGroup<Path>>& GetPathGroups() const {
    return path_groups_;
  }

  void SetTiledPathRange(size_t offset, size_t count);

  void SetCoverageAAFrameData(const CoverageAAFrameData* frame_data);

 private:
  void OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                          HWDrawContext* context) override;

  // The geometry shades all groups with the first group's paint and transform.
  // A future merge implementation must therefore accept only groups with
  // compatible paints and identical transforms, unless per-path local
  // coordinates are added to the GPU data.
  std::vector<BatchGroup<Path>> path_groups_;
  Matrix physical_to_layer_;
  const CoverageAAFrameData* frame_data_ = nullptr;
  size_t tiled_path_offset_ = 0;
  size_t tiled_path_count_ = 0;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_DYNAMIC_COVERAGE_PATH_DRAW_HPP
