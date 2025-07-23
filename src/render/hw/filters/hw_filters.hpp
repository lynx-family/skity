// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_FILTERS_HW_FILTERS_HPP
#define SRC_RENDER_HW_FILTERS_HW_FILTERS_HPP

#include <memory>
#include <vector>

#include "src/render/hw/filters/hw_blur_filter.hpp"
#include "src/render/hw/filters/hw_color_filter.hpp"
#include "src/render/hw/filters/hw_matrix_filter.hpp"
#include "src/render/hw/filters/hw_merge_filter.hpp"

namespace skity {

class HWFilters {
 public:
  static std::shared_ptr<HWFilter> ConvertPaintToHWFilter(const Paint& paint,
                                                          Vec2 scale);

  static std::shared_ptr<HWFilter> Blur(float radius_x, float radius_y,
                                        Vec2 scale,
                                        std::shared_ptr<HWFilter> input);
  static std::shared_ptr<HWFilter> ColorFilter(std::shared_ptr<ColorFilter> cf,
                                               std::shared_ptr<HWFilter> input);

  static std::shared_ptr<HWFilter> Matrix(const Matrix& matrix,
                                          std::shared_ptr<HWFilter> input);

  static std::shared_ptr<HWFilter> DropShadow(float radius_x, float radius_y,
                                              float offset_x, float offset_y,
                                              Vec2 scale, Color color,
                                              std::shared_ptr<HWFilter> input);

  static std::shared_ptr<HWFilter> Merge(
      std::vector<std::shared_ptr<HWFilter>> inputs);

 private:
  static std::shared_ptr<HWFilter> HandleMaskFilter(
      std::shared_ptr<HWFilter> input, std::shared_ptr<MaskFilter> mask_filter,
      Vec2 scale);
  static std::shared_ptr<HWFilter> HandleColorFilter(
      std::shared_ptr<HWFilter> input,
      std::shared_ptr<skity::ColorFilter> color_filter);
  static std::shared_ptr<HWFilter> HandleImageFilter(
      std::shared_ptr<HWFilter> input,
      std::shared_ptr<ImageFilter> image_filter, Vec2 scale);
};
}  // namespace skity

#endif  // SRC_RENDER_HW_FILTERS_HW_FILTERS_HPP
