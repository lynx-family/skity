// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_IMAGE_FILTER_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_IMAGE_FILTER_HPP

// ImageFilter wrapper of the header-only RAII layer: filters the entire
// drawing before it is composited (blur, shadows, morphology, transforms,
// chaining). Factory names keep the legacy C++ shapes so consumer code
// compiles against both APIs unchanged. See skity.hpp for the layer's
// design.

#include <skity_c/skity_image_filter.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_color_filter.hpp>
#include <skity_hpp/skity_types.hpp>

namespace skity {
namespace raii {

/** Image filter wrapper. Attach with Paint::SetImageFilter. */
class ImageFilter
    : public detail::OwnHandle<skity_image_filter, skity_image_filter_destroy> {
 public:
  /** Gaussian blur; @p sigma_x / @p sigma_y in pixels (>= 0). */
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

  /** Transform the filtered result by @p matrix. */
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
   * Drop shadow of @p input: offset by (@p dx, @p dy), blurred by
   * (@p sigma_x, @p sigma_y), drawn in @p color. An empty @p input shadows
   * the source primitive directly; @p crop (optional) clips the output.
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

  /** Wrap @p input with a local matrix applied before the canvas matrix. */
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

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_IMAGE_FILTER_HPP
