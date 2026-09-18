// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_COLOR_FILTER_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_COLOR_FILTER_HPP

// ColorFilter wrapper of the header-only RAII layer: transforms the source
// color of each pixel during drawing. Factory names keep the legacy C++
// shapes (Blend / Compose / Matrix / LinearToSRGBGamma / SRGBToLinearGamma)
// so consumer code compiles against both APIs unchanged. See skity.hpp for
// the layer's design.

#include <skity_c/skity_color_filter.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_types.hpp>

namespace skity {
namespace raii {

/** Color filter wrapper. Attach with Paint::SetColorFilter. */
class ColorFilter
    : public detail::OwnHandle<skity_color_filter, skity_color_filter_destroy> {
 public:
  /** Blend each source pixel with a constant color. */
  static ColorFilter Blend(Color color, BlendMode mode) {
    return ColorFilter(skity_color_filter_create_blend(color, to_c(mode)));
  }

  /** Apply @p outer after @p inner: result = outer(inner(src)). */
  static ColorFilter Compose(const ColorFilter& outer, const ColorFilter& inner) {
    return ColorFilter(
        skity_color_filter_create_compose(outer.get(), inner.get()));
  }

  /** Apply a 4x5 color matrix (20 floats, row-major RGBA + bias). */
  static ColorFilter Matrix(const float row_major[20]) {
    return ColorFilter(skity_color_filter_create_matrix(row_major));
  }

  /** Convert sRGB-encoded pixels to a linear gamma. */
  static ColorFilter LinearToSRGBGamma() {
    return ColorFilter(skity_color_filter_create_linear_to_srgb());
  }

  /** Convert linear-gamma pixels to sRGB encoding. */
  static ColorFilter SRGBToLinearGamma() {
    return ColorFilter(skity_color_filter_create_srgb_to_linear());
  }

  /**
   * Adopt an owning C handle (e.g. from skity_paint_get_color_filter);
   * destroying the wrapper releases the shared reference.
   */
  static ColorFilter Adopt(skity_color_filter h) { return ColorFilter(h); }

 private:
  explicit ColorFilter(skity_color_filter h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_COLOR_FILTER_HPP
