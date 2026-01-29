// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_FILTERS_HW_DOWN_SAMPLER_FILTER_HPP
#define SRC_RENDER_HW_FILTERS_HW_DOWN_SAMPLER_FILTER_HPP

#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/filters/hw_filter.hpp"

namespace skity {

class HWDownSamplerFilter : public HWFilter {
 public:
  HWDownSamplerFilter(std::shared_ptr<HWFilter> input, float scale);

  ~HWDownSamplerFilter() override = default;

  HWFilterOutput Prepare(const HWFilterContext &context) override;

 private:
  void PrepareCMDWGX(HWDrawContext *context, Command *cmd,
                     const std::shared_ptr<GPUTexture> &input_texture,
                     const Vec2 &output_size);

 private:
  float scale_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_FILTERS_HW_DOWN_SAMPLER_FILTER_HPP
