// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_COLOR_FILTER_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_COLOR_FILTER_HPP

#include <skity/c/skity_color_filter.h>

#include <cstdint>
#include <memory>
#include <skity/base.hpp>
#include <skity/types.hpp>

namespace skity {

/**
 * @class ColorFilter
 *
 * Color filter wrapper. Attach with Paint::SetColorFilter.
 *
 */
class ColorFilter
    : public detail::OwnHandle<skity_color_filter, skity_color_filter_destroy> {
 public:
  /** Blend each source pixel with a constant color. */
  static std::shared_ptr<ColorFilter> Blend(Color color, BlendMode mode) {
    auto handle = skity_color_filter_create_blend(color, to_c(mode));
    return handle != nullptr
               ? std::shared_ptr<ColorFilter>(new ColorFilter(handle))
               : nullptr;
  }

  /** Apply outer after inner: result = outer(inner(src)). */
  static std::shared_ptr<ColorFilter> Compose(
      std::shared_ptr<ColorFilter> outer, std::shared_ptr<ColorFilter> inner) {
    auto handle = skity_color_filter_create_compose(outer->get(), inner->get());
    return handle != nullptr
               ? std::shared_ptr<ColorFilter>(new ColorFilter(handle))
               : nullptr;
  }

  /** Apply a 4x5 color matrix (20 floats, row-major RGBA + bias). */
  static std::shared_ptr<ColorFilter> Matrix(const float row_major[20]) {
    auto handle = skity_color_filter_create_matrix(row_major);
    return handle != nullptr
               ? std::shared_ptr<ColorFilter>(new ColorFilter(handle))
               : nullptr;
  }

  /** Convert sRGB-encoded pixels to a linear gamma. */
  static std::shared_ptr<ColorFilter> LinearToSRGBGamma() {
    auto handle = skity_color_filter_create_linear_to_srgb();
    return handle != nullptr
               ? std::shared_ptr<ColorFilter>(new ColorFilter(handle))
               : nullptr;
  }

  /** Convert linear-gamma pixels to sRGB encoding. */
  static std::shared_ptr<ColorFilter> SRGBToLinearGamma() {
    auto handle = skity_color_filter_create_srgb_to_linear();
    return handle != nullptr
               ? std::shared_ptr<ColorFilter>(new ColorFilter(handle))
               : nullptr;
  }

  /**
   * Adopt an owning C handle (e.g. from skity_paint_get_color_filter);
   * destroying the wrapper releases the shared reference.
   */
  static ColorFilter Adopt(skity_color_filter h) { return ColorFilter(h); }

  friend class Paint;

 private:
  explicit ColorFilter(skity_color_filter h) : OwnHandle(h) {}
};

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_COLOR_FILTER_HPP
