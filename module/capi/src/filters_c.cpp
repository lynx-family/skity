// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_color_filter.h>
#include <skity_c/skity_image_filter.h>
#include <skity_c/skity_mask_filter.h>
#include <skity_c/skity_path_effect.h>

#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>
#include <skity/effect/path_effect.hpp>
#include <skity/geometry/matrix.hpp>

#include "handle.hpp"

extern "C" {

/* ----- color filter ----- */

skity_color_filter skity_color_filter_create_blend(skity_color color,
                                                   skity_blend_mode mode) {
  auto sp = skity::ColorFilters::Blend(static_cast<skity::Color>(color),
                                       static_cast<skity::BlendMode>(mode));
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_color_filter_s>(
      SKITY_OBJECT_TYPE_COLOR_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_color_filter skity_color_filter_create_compose(skity_color_filter outer,
                                                     skity_color_filter inner) {
  auto o = skity::capi::get_impl<skity_color_filter_s, skity::ColorFilter>(
      outer, SKITY_OBJECT_TYPE_COLOR_FILTER);
  auto i = skity::capi::get_impl<skity_color_filter_s, skity::ColorFilter>(
      inner, SKITY_OBJECT_TYPE_COLOR_FILTER);
  auto sp = skity::ColorFilters::Compose(o, i);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_color_filter_s>(
      SKITY_OBJECT_TYPE_COLOR_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_color_filter skity_color_filter_create_matrix(const float* row_major) {
  if (row_major == nullptr) return nullptr;
  auto sp = skity::ColorFilters::Matrix(row_major);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_color_filter_s>(
      SKITY_OBJECT_TYPE_COLOR_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_color_filter skity_color_filter_create_linear_to_srgb(void) {
  auto sp = skity::ColorFilters::LinearToSRGBGamma();
  return skity::capi::alloc_handle<skity_color_filter_s>(
      SKITY_OBJECT_TYPE_COLOR_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_color_filter skity_color_filter_create_srgb_to_linear(void) {
  auto sp = skity::ColorFilters::SRGBToLinearGamma();
  return skity::capi::alloc_handle<skity_color_filter_s>(
      SKITY_OBJECT_TYPE_COLOR_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

void skity_color_filter_destroy(skity_color_filter filter) {
  skity::capi::destroy_handle<skity_color_filter_s>(
      filter, SKITY_OBJECT_TYPE_COLOR_FILTER);
}

/* ----- image filter ----- */

skity_image_filter skity_image_filter_create_blur(float sigma_x,
                                                  float sigma_y) {
  auto sp = skity::ImageFilters::Blur(sigma_x, sigma_y);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_image_filter_s>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_image_filter skity_image_filter_create_dilate(float radius_x,
                                                    float radius_y) {
  auto sp = skity::ImageFilters::Dilate(radius_x, radius_y);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_image_filter_s>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_image_filter skity_image_filter_create_erode(float radius_x,
                                                   float radius_y) {
  auto sp = skity::ImageFilters::Erode(radius_x, radius_y);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_image_filter_s>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_image_filter skity_image_filter_create_matrix_transform(
    const skity_matrix* matrix) {
  if (matrix == nullptr) return nullptr;
  auto sp = skity::ImageFilters::MatrixTransform(
      *reinterpret_cast<const skity::Matrix*>(matrix));
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_image_filter_s>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_image_filter skity_image_filter_create_from_color_filter(
    skity_color_filter filter) {
  auto cf = skity::capi::get_impl<skity_color_filter_s, skity::ColorFilter>(
      filter, SKITY_OBJECT_TYPE_COLOR_FILTER);
  auto sp = skity::ImageFilters::ColorFilter(cf);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_image_filter_s>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_image_filter skity_image_filter_create_compose(skity_image_filter outer,
                                                     skity_image_filter inner) {
  auto o = skity::capi::get_impl<skity_image_filter_s, skity::ImageFilter>(
      outer, SKITY_OBJECT_TYPE_IMAGE_FILTER);
  auto i = skity::capi::get_impl<skity_image_filter_s, skity::ImageFilter>(
      inner, SKITY_OBJECT_TYPE_IMAGE_FILTER);
  auto sp = skity::ImageFilters::Compose(o, i);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_image_filter_s>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_image_filter skity_image_filter_create_drop_shadow(
    float dx, float dy, float sigma_x, float sigma_y, skity_color color,
    skity_image_filter input, const skity_rect* crop) {
  auto in = skity::capi::get_impl<skity_image_filter_s, skity::ImageFilter>(
      input, SKITY_OBJECT_TYPE_IMAGE_FILTER);
  skity::Rect c = crop != nullptr ? *reinterpret_cast<const skity::Rect*>(crop)
                                  : skity::Rect{};
  auto sp = skity::ImageFilters::DropShadow(
      dx, dy, sigma_x, sigma_y, static_cast<skity::Color>(color), in, c);
  if (sp == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_image_filter_s>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_image_filter skity_image_filter_create_local_matrix(
    skity_image_filter input, const skity_matrix* matrix) {
  auto in = skity::capi::get_impl<skity_image_filter_s, skity::ImageFilter>(
      input, SKITY_OBJECT_TYPE_IMAGE_FILTER);
  if (in == nullptr || matrix == nullptr) {
    return nullptr;
  }
  auto sp = skity::ImageFilters::LocalMatrix(
      in, *reinterpret_cast<const skity::Matrix*>(matrix));
  if (sp == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_image_filter_s>(
      SKITY_OBJECT_TYPE_IMAGE_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

void skity_image_filter_destroy(skity_image_filter filter) {
  skity::capi::destroy_handle<skity_image_filter_s>(
      filter, SKITY_OBJECT_TYPE_IMAGE_FILTER);
}

/* ----- mask filter ----- */

skity_mask_filter skity_mask_filter_create_blur(skity_blur_style style,
                                                float radius) {
  auto sp =
      skity::MaskFilter::MakeBlur(static_cast<skity::BlurStyle>(style), radius);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_mask_filter_s>(
      SKITY_OBJECT_TYPE_MASK_FILTER, SKITY_HANDLE_OWNING, std::move(sp));
}

void skity_mask_filter_destroy(skity_mask_filter filter) {
  skity::capi::destroy_handle<skity_mask_filter_s>(
      filter, SKITY_OBJECT_TYPE_MASK_FILTER);
}

/* ----- path effect ----- */

skity_path_effect skity_path_effect_create_discrete(float seg_length, float dev,
                                                    uint32_t seed_assist) {
  auto sp =
      skity::PathEffect::MakeDiscretePathEffect(seg_length, dev, seed_assist);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_path_effect_s>(
      SKITY_OBJECT_TYPE_PATH_EFFECT, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_path_effect skity_path_effect_create_dash(const float* intervals,
                                                int32_t count, float phase) {
  if (intervals == nullptr || count <= 0) return nullptr;
  auto sp = skity::PathEffect::MakeDashPathEffect(intervals, count, phase);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_path_effect_s>(
      SKITY_OBJECT_TYPE_PATH_EFFECT, SKITY_HANDLE_OWNING, std::move(sp));
}

void skity_path_effect_destroy(skity_path_effect effect) {
  skity::capi::destroy_handle<skity_path_effect_s>(
      effect, SKITY_OBJECT_TYPE_PATH_EFFECT);
}

}  // extern "C"
