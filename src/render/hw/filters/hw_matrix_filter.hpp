// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_FILTERS_HW_MATRIX_FILTER_HPP
#define SRC_RENDER_HW_FILTERS_HW_MATRIX_FILTER_HPP

#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/filters/hw_filter.hpp"

namespace skity {
class HWMatrixFilter : public HWFilter {
 public:
  HWMatrixFilter(const Matrix& matrix, std::shared_ptr<HWFilter> inputs)
      : HWFilter({inputs}, "HWMatrixFilter"), matrix_(matrix) {}

  HWFilterOutput Prepare(const HWFilterContext& context) override;

 private:
  Matrix matrix_;
};
}  // namespace skity

#endif  // SRC_RENDER_HW_FILTERS_HW_MATRIX_FILTER_HPP
