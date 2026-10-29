// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_IMAGE_FILTER_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_IMAGE_FILTER_HPP

#include <skity/c/skity_image_filter.h>

#include <cstdint>
#include <memory>
#include <skity/base.hpp>
#include <skity/effect/color_filter.hpp>
#include <skity/types.hpp>

namespace skity {

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
  static std::shared_ptr<ImageFilter> Blur(float sigma_x, float sigma_y) {
    auto handle = skity_image_filter_create_blur(sigma_x, sigma_y);
    return handle != nullptr
               ? std::shared_ptr<ImageFilter>(new ImageFilter(handle))
               : nullptr;
  }

  /** Morphology dilate (max); radii in pixels. */
  static std::shared_ptr<ImageFilter> Dilate(float radius_x, float radius_y) {
    auto handle = skity_image_filter_create_dilate(radius_x, radius_y);
    return handle != nullptr
               ? std::shared_ptr<ImageFilter>(new ImageFilter(handle))
               : nullptr;
  }

  /** Morphology erode (min); radii in pixels. */
  static std::shared_ptr<ImageFilter> Erode(float radius_x, float radius_y) {
    auto handle = skity_image_filter_create_erode(radius_x, radius_y);
    return handle != nullptr
               ? std::shared_ptr<ImageFilter>(new ImageFilter(handle))
               : nullptr;
  }

  /** Transform the filtered result by matrix. */
  static std::shared_ptr<ImageFilter> MatrixTransform(const Matrix& matrix) {
    auto handle = skity_image_filter_create_matrix_transform(&matrix);
    return handle != nullptr
               ? std::shared_ptr<ImageFilter>(new ImageFilter(handle))
               : nullptr;
  }

  /** Wrap a color filter so it can be chained / attached as an image filter. */
  static std::shared_ptr<ImageFilter> ColorFilter(
      std::shared_ptr<ColorFilter> filter) {
    auto handle = skity_image_filter_create_from_color_filter(filter->get());
    return handle != nullptr
               ? std::shared_ptr<ImageFilter>(new ImageFilter(handle))
               : nullptr;
  }

  /** Chain two filters: result = outer(inner(src)). */
  static std::shared_ptr<ImageFilter> Compose(
      std::shared_ptr<ImageFilter> outer, std::shared_ptr<ImageFilter> inner) {
    auto handle = skity_image_filter_create_compose(outer->get(), inner->get());
    return handle != nullptr
               ? std::shared_ptr<ImageFilter>(new ImageFilter(handle))
               : nullptr;
  }

  /**
   * Drop shadow of input: offset by (dx, dy), blurred by
   * (sigma_x, sigma_y), drawn in color. An empty input shadows
   * the source primitive directly; crop (optional) clips the output.
   */
  static std::shared_ptr<ImageFilter> DropShadow(
      float dx, float dy, float sigma_x, float sigma_y, Color color,
      std::shared_ptr<ImageFilter> input) {
    auto handle = skity_image_filter_create_drop_shadow(
        dx, dy, sigma_x, sigma_y, color, input ? input->get() : nullptr,
        nullptr);
    return handle != nullptr
               ? std::shared_ptr<ImageFilter>(new ImageFilter(handle))
               : nullptr;
  }
  /** Drop shadow of the source primitive directly (no input filter, no crop).
   */
  static std::shared_ptr<ImageFilter> DropShadow(float dx, float dy,
                                                 float sigma_x, float sigma_y,
                                                 Color color) {
    auto handle = skity_image_filter_create_drop_shadow(
        dx, dy, sigma_x, sigma_y, color, nullptr, nullptr);
    return handle != nullptr
               ? std::shared_ptr<ImageFilter>(new ImageFilter(handle))
               : nullptr;
  }
  /** Drop-shadow variant with an explicit crop rectangle on the output. */
  static std::shared_ptr<ImageFilter> DropShadow(
      float dx, float dy, float sigma_x, float sigma_y, Color color,
      std::shared_ptr<ImageFilter> input, const Rect& crop) {
    auto handle = skity_image_filter_create_drop_shadow(
        dx, dy, sigma_x, sigma_y, color, input ? input->get() : nullptr, &crop);
    return handle != nullptr
               ? std::shared_ptr<ImageFilter>(new ImageFilter(handle))
               : nullptr;
  }

  /** Wrap input with a local matrix applied before the canvas matrix. */
  static std::shared_ptr<ImageFilter> LocalMatrix(
      std::shared_ptr<ImageFilter> input, const Matrix& matrix) {
    auto handle = skity_image_filter_create_local_matrix(input->get(), &matrix);
    return handle != nullptr
               ? std::shared_ptr<ImageFilter>(new ImageFilter(handle))
               : nullptr;
  }

  /**
   * Adopt an owning C handle (e.g. from skity_paint_get_image_filter);
   * destroying the wrapper releases the shared reference.
   */
  static ImageFilter Adopt(skity_image_filter h) { return ImageFilter(h); }

  friend class Paint;

 private:
  explicit ImageFilter(skity_image_filter h) : OwnHandle(h) {}
};

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_IMAGE_FILTER_HPP
