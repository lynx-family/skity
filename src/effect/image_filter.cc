/*
 * Copyright 2012 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/effect/mask_filter.hpp>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/render/canvas.hpp>

#include "src/effect/image_filter_base.hpp"

#if defined(SKITY_CPU)
#include <cstring>

#include "src/graphic/color_priv.hpp"
#include "src/render/sw/sw_stack_blur.hpp"
#endif

namespace skity {

// static
Rect ImageFilterBase::ApproximateFilteredBounds(const Rect& src, float radius_x,
                                                float radius_y) {
  float l = std::floor(src.Left() - radius_x);
  float t = std::floor(src.Top() - radius_y);
  float r = std::ceil(src.Right() + radius_x);
  float b = std::ceil(src.Bottom() + radius_y);

  return Rect::MakeLTRB(l, t, r, b);
}

std::shared_ptr<ImageFilter> ImageFilters::Blur(float sigma_x, float sigma_y) {
  // ScalarNearlyZero or 0.5?
  if (sigma_x <= 0.5 && sigma_y <= 0.5) {
    return nullptr;
  }
  return std::make_shared<BlurImageFilter>(sigma_x, sigma_y);
}

std::shared_ptr<ImageFilter> ImageFilters::DropShadow(
    float dx, float dy, float sigma_x, float sigma_y, Color color,
    std::shared_ptr<ImageFilter> input, const Rect& crop_rect) {
  if (sigma_x <= 0.5 && sigma_y <= 0.5) {
    return nullptr;
  }
  return std::make_shared<DropShadowImageFilter>(dx, dy, sigma_x, sigma_y,
                                                 color, input, crop_rect);
}

std::shared_ptr<ImageFilter> ImageFilters::Dilate(float radius_x,
                                                  float radius_y) {
  if (radius_x < 0 || radius_y < 0) {
    return nullptr;
  }
  return std::make_shared<MorphologyImageFilter>(ImageFilterType::kDilate,
                                                 radius_x, radius_y);
}

std::shared_ptr<ImageFilter> ImageFilters::Erode(float radius_x,
                                                 float radius_y) {
  if (radius_x < 0 || radius_y < 0) {
    return nullptr;
  }
  return std::make_shared<MorphologyImageFilter>(ImageFilterType::kErode,
                                                 radius_x, radius_y);
}

std::shared_ptr<ImageFilter> ImageFilters::MatrixTransform(
    const Matrix& matrix) {
  return std::make_shared<MatrixImageFilter>(matrix);
}

std::shared_ptr<ImageFilter> ImageFilters::ColorFilter(
    std::shared_ptr<class ColorFilter> color_filter) {
  return std::make_shared<ColorFilterImageFilter>(color_filter);
}

std::shared_ptr<ImageFilter> ImageFilters::Compose(
    std::shared_ptr<ImageFilter> outer, std::shared_ptr<ImageFilter> inner) {
  return std::make_shared<ComposeImageFilter>(outer, inner);
}

std::shared_ptr<ImageFilter> ImageFilters::LocalMatrix(
    std::shared_ptr<ImageFilter> image_filter, const Matrix& local_matrix) {
  if (!image_filter) {
    return nullptr;
  }

  if (local_matrix.IsIdentity()) {
    return image_filter;
  }

  ImageFilterBase* image_filter_base =
      static_cast<ImageFilterBase*>(image_filter.get());
  switch (image_filter_base->GetType()) {
    case ImageFilterType::kIdentity:
      return image_filter;
    case ImageFilterType::kBlur: {
      if (local_matrix.OnlyScaleAndTranslate()) {
        BlurImageFilter* blur_image_filter =
            static_cast<BlurImageFilter*>(image_filter_base);
        float radius_x =
            local_matrix.GetScaleX() * blur_image_filter->GetRadiusX();
        float radius_y =
            local_matrix.GetScaleX() * blur_image_filter->GetRadiusY();
        return ImageFilters::Blur(ConvertRadiusToSigma(radius_x),
                                  ConvertRadiusToSigma(radius_y));
      }
      return nullptr;
    }
    case ImageFilterType::kDropShadow: {
      if (local_matrix.OnlyScaleAndTranslate()) {
        DropShadowImageFilter* drop_shadow_image_filter =
            static_cast<DropShadowImageFilter*>(image_filter_base);
        float scale_x = local_matrix.GetScaleX();
        float scale_y = local_matrix.GetScaleY();
        float dx = scale_x * drop_shadow_image_filter->GetOffsetX();
        float dy = scale_y * drop_shadow_image_filter->GetOffsetY();
        float radius_x = scale_x * drop_shadow_image_filter->GetRadiusX();
        float radius_y = scale_y * drop_shadow_image_filter->GetRadiusY();
        return ImageFilters::DropShadow(dx, dy, ConvertRadiusToSigma(radius_x),
                                        ConvertRadiusToSigma(radius_y),
                                        drop_shadow_image_filter->GetColor(),
                                        nullptr);
      }
      return nullptr;
    }
    case ImageFilterType::kColorFilter:
      return image_filter;
    case ImageFilterType::kMatix: {
      MatrixImageFilter* matrix_image_filter =
          static_cast<MatrixImageFilter*>(image_filter_base);
      Matrix inv_local_matrx;
      if (local_matrix.Invert(&inv_local_matrx)) {
        Matrix matrix =
            local_matrix * matrix_image_filter->GetMatrix() * inv_local_matrx;
        return ImageFilters::MatrixTransform(matrix);
      }
      return nullptr;
    }
    case ImageFilterType::kCompose: {
      ComposeImageFilter* compose_image_filter =
          static_cast<ComposeImageFilter*>(image_filter_base);
      if (!compose_image_filter->GetInner()) {
        return ImageFilters::LocalMatrix(compose_image_filter->GetOuter(),
                                         local_matrix);
      }
      if (!compose_image_filter->GetOuter()) {
        return ImageFilters::LocalMatrix(compose_image_filter->GetInner(),
                                         local_matrix);
      }

      auto outer = ImageFilters::LocalMatrix(compose_image_filter->GetOuter(),
                                             local_matrix);
      auto inner = ImageFilters::LocalMatrix(compose_image_filter->GetInner(),
                                             local_matrix);

      if (outer && inner) {
        return ImageFilters::Compose(outer, inner);
      }
      return nullptr;
    }
    case ImageFilterType::kDilate:
    case ImageFilterType::kErode:
      return nullptr;
  }
  return nullptr;
}

#if defined(SKITY_CPU)

// static
void ImageFilterBase::BlurBitmapToCanvas(Canvas* canvas, Bitmap& bitmap,
                                         const Rect& filter_bounds,
                                         const Paint& paint, float radius_x,
                                         float radius_y) {
  Bitmap filtered_bitmap(bitmap.Width(), bitmap.Height(), kPremul_AlphaType);
  SWStackBlur(&bitmap, &filtered_bitmap,
              std::round(std::max(radius_x, radius_y)))
      .Blur();
  canvas->DrawImage(Image::MakeImage(filtered_bitmap.GetPixmap()),
                    filter_bounds, &paint);
}

void BlurImageFilter::OnFilter(Canvas* canvas, Bitmap& bitmap,
                               const Rect& filter_bounds,
                               const Paint& paint) const {
  BlurBitmapToCanvas(canvas, bitmap, filter_bounds, paint, radius_x_,
                     radius_y_);
}

void DropShadowImageFilter::OnFilter(Canvas* canvas, Bitmap& bitmap,
                                     const Rect& filter_bounds,
                                     const Paint& paint) const {
  Bitmap filtered_bitmap(bitmap.Width(), bitmap.Height());
  SWStackBlur(&bitmap, &filtered_bitmap,
              std::round(std::max(radius_x_, radius_y_)))
      .Blur();

  for (size_t y = 0; y < bitmap.Height(); ++y) {
    for (size_t x = 0; x < bitmap.Width(); ++x) {
      auto a = ColorGetA(filtered_bitmap.GetPixel(x, y));
      auto c = a > 0 ? ColorSetA(color_, a) : Color_TRANSPARENT;
      filtered_bitmap.SetPixel(x, y, c);
    }
  }

  Rect offset_bounds = filter_bounds;
  offset_bounds.Offset(GetOffsetX(), GetOffsetY());
  canvas->DrawImage(Image::MakeImage(filtered_bitmap.GetPixmap()),
                    offset_bounds, &paint);

  canvas->DrawImage(Image::MakeImage(bitmap.GetPixmap()), filter_bounds,
                    &paint);
}

enum class MorphDirection { kX, kY };

static void call_proc_X(MorphologyImageFilter::Proc procX, const Bitmap& src,
                        Bitmap* dst, int radiusX, const Rect& bounds) {
  auto src_addr = reinterpret_cast<const PMColor*>(
      src.GetPixelAddr() + int32_t(bounds.Top()) * src.GetPixmap()->RowBytes() +
      int32_t(bounds.Left()));
  auto dst_addr = reinterpret_cast<PMColor*>(dst->GetPixelAddr());
  procX(src_addr, dst_addr, radiusX, bounds.Width(), bounds.Height(),
        src.GetPixmap()->RowBytes() / 4, dst->GetPixmap()->RowBytes() / 4);
}

static void call_proc_Y(MorphologyImageFilter::Proc procY, const PMColor* src,
                        int srcRowBytesAsPixels, Bitmap* dst, int radiusY,
                        const Rect& bounds) {
  procY(src, reinterpret_cast<PMColor*>(dst->GetPixelAddr()), radiusY,
        bounds.Height(), bounds.Width(), srcRowBytesAsPixels,
        dst->GetPixmap()->RowBytes() / 4);
}

template <ImageFilterType type, MorphDirection direction>
static void morph(const PMColor* src, PMColor* dst, int radius, int width,
                  int height, int srcStride, int dstStride) {
  const int srcStrideX = direction == MorphDirection::kX ? 1 : srcStride;
  const int dstStrideX = direction == MorphDirection::kX ? 1 : dstStride;
  const int srcStrideY = direction == MorphDirection::kX ? srcStride : 1;
  const int dstStrideY = direction == MorphDirection::kX ? dstStride : 1;
  radius = std::min(radius, width - 1);
  const PMColor* upperSrc = src + radius * srcStrideX;
  for (int x = 0; x < width; ++x) {
    const PMColor* lp = src;
    const PMColor* up = upperSrc;
    PMColor* dptr = dst;
    for (int y = 0; y < height; ++y) {
      // If we're maxing (dilate), start from 0; if minning (erode), start
      // from 255.
      const int start = (type == ImageFilterType::kDilate) ? 0 : 255;
      int B = start, G = start, R = start, A = start;
      for (const PMColor* p = lp; p <= up; p += srcStrideX) {
        int b = ColorGetB(*p), g = ColorGetG(*p), r = ColorGetR(*p),
            a = ColorGetA(*p);
        if (type == ImageFilterType::kDilate) {
          B = std::max(b, B);
          G = std::max(g, G);
          R = std::max(r, R);
          A = std::max(a, A);
        } else {
          B = std::min(b, B);
          G = std::min(g, G);
          R = std::min(r, R);
          A = std::min(a, A);
        }
      }
      *dptr = ColorSetARGB(A, R, G, B);
      dptr += dstStrideY;
      lp += srcStrideY;
      up += srcStrideY;
    }
    if (x >= radius) {
      src += srcStrideX;
    }
    if (x + radius < width - 1) {
      upperSrc += srcStrideX;
    }
    dst += dstStrideX;
  }
}

void MorphologyImageFilter::OnFilter(Canvas* canvas, Bitmap& bitmap,
                                     const Rect& filter_bounds,
                                     const Paint& paint) const {
  Bitmap filtered_bitmap(filter_bounds.Width(), filter_bounds.Height());

  Proc procX, procY;

  if (ImageFilterType::kDilate == GetType()) {
    procX = &morph<ImageFilterType::kDilate, MorphDirection::kX>;
    procY = &morph<ImageFilterType::kDilate, MorphDirection::kY>;
  } else {
    procX = &morph<ImageFilterType::kErode, MorphDirection::kX>;
    procY = &morph<ImageFilterType::kErode, MorphDirection::kY>;
  }

  Rect src_bounds = Rect::MakeWH(filter_bounds.Width(), filter_bounds.Height());
  if (radius_x_ > 0 && radius_y_ > 0) {
    Bitmap tmp(filter_bounds.Width(), filter_bounds.Height(),
               AlphaType::kPremul_AlphaType);

    call_proc_X(procX, bitmap, &tmp, radius_x_, src_bounds);
    Rect tmp_bounds = Rect::MakeWH(src_bounds.Width(), src_bounds.Height());
    call_proc_Y(procY, (const skity::PMColor*)tmp.GetPixelAddr(),
                tmp.GetPixmap()->RowBytes() / 4, &filtered_bitmap, radius_y_,
                tmp_bounds);
  } else if (radius_x_ > 0) {
    call_proc_X(procX, bitmap, &filtered_bitmap, radius_x_, src_bounds);
  } else if (radius_y_ > 0) {
    auto src_addr = reinterpret_cast<const skity::PMColor*>(
        bitmap.GetPixelAddr() +
        int32_t(src_bounds.Top()) * bitmap.GetPixmap()->RowBytes() +
        int32_t(src_bounds.Left()) * 4);
    call_proc_Y(procY, src_addr, bitmap.GetPixmap()->RowBytes() / 4,
                &filtered_bitmap, radius_y_, src_bounds);
  }

  canvas->DrawImage(Image::MakeImage(filtered_bitmap.GetPixmap()),
                    filter_bounds, &paint);
}

#endif

MatrixImageFilter::MatrixImageFilter(const Matrix& matrix) : matrix_(matrix) {}

ColorFilterImageFilter::ColorFilterImageFilter(
    std::shared_ptr<ColorFilter> cf) {
  color_filter_ = cf;
}

}  // namespace skity
