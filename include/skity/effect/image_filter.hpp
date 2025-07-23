// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_EFFECT_IMAGE_FILTER_HPP
#define INCLUDE_SKITY_EFFECT_IMAGE_FILTER_HPP

#include <memory>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/point.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/color.hpp>
#include <skity/macros.hpp>

namespace skity {
class ColorFilter;
class Pixmap;

class SKITY_API ImageFilter {
 public:
  virtual ~ImageFilter() = default;

  virtual Rect ComputeFastBounds(const Rect& src) const { return src; }

 private:
  ImageFilter() = default;
  ImageFilter& operator=(const ImageFilter&) = delete;
  friend class ImageFilterBase;
};

class SKITY_API ImageFilters {
 public:
  static std::shared_ptr<ImageFilter> Blur(float sigma_x, float sigma_y);

  static std::shared_ptr<ImageFilter> DropShadow(
      float dx, float dy, float sigma_x, float sigma_y, Color color,
      std::shared_ptr<ImageFilter> input, const Rect& cropRect = {});

  static std::shared_ptr<ImageFilter> Dilate(float radius_x, float radius_y);

  static std::shared_ptr<ImageFilter> Erode(float radius_x, float radius_y);

  static std::shared_ptr<ImageFilter> MatrixTransform(const Matrix& matrix);

  static std::shared_ptr<ImageFilter> ColorFilter(
      std::shared_ptr<skity::ColorFilter> cf);

  static std::shared_ptr<ImageFilter> Compose(
      std::shared_ptr<ImageFilter> outer, std::shared_ptr<ImageFilter> inner);

  static std::shared_ptr<ImageFilter> LocalMatrix(
      std::shared_ptr<ImageFilter> image_filter, const Matrix& local_matrix);
};

}  // namespace skity

#endif  // INCLUDE_SKITY_EFFECT_IMAGE_FILTER_HPP
