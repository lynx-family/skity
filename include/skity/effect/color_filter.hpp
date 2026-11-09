// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_EFFECT_COLOR_FILTER_HPP
#define INCLUDE_SKITY_EFFECT_COLOR_FILTER_HPP

#include <memory>
#include <skity/graphic/blend_mode.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/io/flattenable.hpp>

namespace skity {

class SKITY_API ColorFilter : public Flattenable {
 public:
  /**
   * Filters a color; the filter math runs in the target (sRGB) space, see
   * the color space design notes for the space semantics of each filter.
   */
  PMColor FilterColor(PMColor) const;

  /**
   * Same as above, but declares the render target's color space the filter
   * math runs in (filters carrying recorded colors resolve them into this
   * space first).
   */
  PMColor FilterColor(PMColor, const ColorSpace& target_space) const;
  ~ColorFilter() override = default;

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

  /**
   * Same as above with the blend color recorded in color_space (default
   * sRGB); the color resolves into the render target's space when the
   * filter is applied.
   */
  static std::shared_ptr<ColorFilter> Blend(Color4f color, BlendMode mode,
                                            ColorSpace color_space = {});

  static std::shared_ptr<ColorFilter> Matrix(const float row_major[20]);

  static std::shared_ptr<ColorFilter> LinearToSRGBGamma();

  static std::shared_ptr<ColorFilter> SRGBToLinearGamma();

 private:
  ColorFilters() = delete;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_EFFECT_COLOR_FILTER_HPP
