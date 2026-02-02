// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_FILTERS_HW_BLUR_FILTER_HPP
#define SRC_RENDER_HW_FILTERS_HW_BLUR_FILTER_HPP

#include <memory>

#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/filters/hw_filter.hpp"

namespace skity {

class HWBlurFilter : public HWFilter {
 public:
  HWBlurFilter(float radius, Vec2 direction, std::shared_ptr<HWFilter> inputs)
      : HWFilter({inputs}, "HWBlurFilter"),
        radius_(radius),
        direction_(direction) {}

  float radius_;
  Vec2 direction_;

  HWFilterOutput Prepare(const HWFilterContext &context) override;

 private:
  void PrepareWGXCMD(Command *cmd, HWDrawContext *context,
                     const std::shared_ptr<GPUTexture> &texture,
                     const std::shared_ptr<GPUTexture> &output_texture,
                     const Vec2 &dir, float radius, Vec2 uv_scale,
                     Vec2 uv_offset);
};

}  // namespace skity

#endif  // SRC_RENDER_HW_FILTERS_HW_BLUR_FILTER_HPP
