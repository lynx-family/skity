// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_EFFECT_IMAGE_FILTER_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_EFFECT_IMAGE_FILTER_HPP

#include <skity_c/skity_image_filter.h>

#include <cstdint>
#include <skity_hpp/base.hpp>
#include <skity_hpp/effect/color_filter.hpp>
#include <skity_hpp/types.hpp>

namespace skity {
namespace raii {

/**
 * @class ImageFilter
 *
 * Image filter wrapper. Attach with Paint::SetImageFilter.
 *
 */
class ImageFilter
    : public detail::OwnHandle<skity_image_filter, skity_image_filter_destroy> {
 public:
  /** Gaussian blur; sigma_x / sigma_y in pixels (>= 0). */
  static ImageFilter Blur(float sigma_x, float sigma_y) {
    return ImageFilter(skity_image_filter_create_blur(sigma_x, sigma_y));
  }

  /** Morphology dilate (max); radii in pixels. */
  static ImageFilter Dilate(float radius_x, float radius_y) {
    return ImageFilter(skity_image_filter_create_dilate(radius_x, radius_y));
  }

  /** Morphology erode (min); radii in pixels. */
  static ImageFilter Erode(float radius_x, float radius_y) {
    return ImageFilter(skity_image_filter_create_erode(radius_x, radius_y));
  }

  /** Transform the filtered result by matrix. */
  static ImageFilter MatrixTransform(const Matrix& matrix) {
    return ImageFilter(skity_image_filter_create_matrix_transform(&matrix));
  }

  /** Wrap a color filter so it can be chained / attached as an image filter. */
  static ImageFilter ColorFilter(const raii::ColorFilter& filter) {
    return ImageFilter(
        skity_image_filter_create_from_color_filter(filter.get()));
  }

  /** Chain two filters: result = outer(inner(src)). */
  static ImageFilter Compose(const ImageFilter& outer,
                             const ImageFilter& inner) {
    return ImageFilter(
        skity_image_filter_create_compose(outer.get(), inner.get()));
  }

  /**
   * Drop shadow of input: offset by (dx, dy), blurred by
   * (sigma_x, sigma_y), drawn in color. An empty input shadows
   * the source primitive directly; crop (optional) clips the output.
   */
  static ImageFilter DropShadow(float dx, float dy, float sigma_x,
                                float sigma_y, Color color,
                                const ImageFilter& input) {
    return ImageFilter(skity_image_filter_create_drop_shadow(
        dx, dy, sigma_x, sigma_y, color, input.get(), nullptr));
  }
  /** Drop shadow of the source primitive directly (no input filter, no crop).
   */
  static ImageFilter DropShadow(float dx, float dy, float sigma_x,
                                float sigma_y, Color color) {
    return ImageFilter(skity_image_filter_create_drop_shadow(
        dx, dy, sigma_x, sigma_y, color, nullptr, nullptr));
  }
  /** Drop-shadow variant with an explicit crop rectangle on the output. */
  static ImageFilter DropShadow(float dx, float dy, float sigma_x,
                                float sigma_y, Color color,
                                const ImageFilter& input, const Rect& crop) {
    return ImageFilter(skity_image_filter_create_drop_shadow(
        dx, dy, sigma_x, sigma_y, color, input.get(), &crop));
  }

  /** Wrap input with a local matrix applied before the canvas matrix. */
  static ImageFilter LocalMatrix(const ImageFilter& input,
                                 const Matrix& matrix) {
    return ImageFilter(
        skity_image_filter_create_local_matrix(input.get(), &matrix));
  }

  /**
   * Adopt an owning C handle (e.g. from skity_paint_get_image_filter);
   * destroying the wrapper releases the shared reference.
   */
  static ImageFilter Adopt(skity_image_filter h) { return ImageFilter(h); }

 private:
  explicit ImageFilter(skity_image_filter h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_EFFECT_IMAGE_FILTER_HPP
