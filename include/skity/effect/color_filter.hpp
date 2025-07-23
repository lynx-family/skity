// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_EFFECT_COLOR_FILTER_HPP
#define INCLUDE_SKITY_EFFECT_COLOR_FILTER_HPP

#include <memory>
#include <skity/graphic/blend_mode.hpp>
#include <skity/graphic/color.hpp>

namespace skity {

class SKITY_API ColorFilter {
 public:
  PMColor FilterColor(PMColor) const;
  ~ColorFilter() = default;

 private:
  ColorFilter() = default;
  ColorFilter& operator=(const ColorFilter&) = delete;
  friend class ColorFilterBase;
};

class SKITY_API ColorFilters {
 public:
  static std::shared_ptr<ColorFilter> Compose(
      std::shared_ptr<ColorFilter> outer, std::shared_ptr<ColorFilter> inner);

  static std::shared_ptr<ColorFilter> Blend(Color color, BlendMode mode);

  static std::shared_ptr<ColorFilter> Matrix(const float row_major[20]);

  static std::shared_ptr<ColorFilter> LinearToSRGBGamma();

  static std::shared_ptr<ColorFilter> SRGBToLinearGamma();

 private:
  ColorFilters() = delete;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_EFFECT_COLOR_FILTER_HPP
