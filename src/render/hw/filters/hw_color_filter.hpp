// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_FILTERS_HW_COLOR_FILTER_HPP
#define SRC_RENDER_HW_FILTERS_HW_COLOR_FILTER_HPP

#include <memory>
#include <skity/effect/color_filter.hpp>

#include "src/render/hw/filters/hw_filter.hpp"

namespace skity {
class HWColorFilter : public HWFilter {
 public:
  HWColorFilter(std::shared_ptr<ColorFilter> color_filter,
                std::shared_ptr<HWFilter> inputs)
      : HWFilter({inputs}), color_filter_(color_filter) {}

  HWFilterOutput DoFilter(const HWFilterContext &context,
                          GPUCommandBuffer *command_buffer) override;

  void PrepareCMD(HWDrawContext *context, Command *cmd,
                  const std::shared_ptr<GPUTexture> &input_texture);

 private:
  void InternalPrepareCMDWGX(HWDrawContext *context, Command *cmd,
                             const std::shared_ptr<GPUTexture> &input_texture);

 private:
  std::shared_ptr<ColorFilter> color_filter_;
};
}  // namespace skity

#endif  // SRC_RENDER_HW_FILTERS_HW_COLOR_FILTER_HPP
