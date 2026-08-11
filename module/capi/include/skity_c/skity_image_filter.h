// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_IMAGE_FILTER_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_IMAGE_FILTER_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_color_filter.h>
#include <skity_c/skity_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ImageFilter: filters the entire drawing before it is composited,
 *        operating on the source image rather than per-pixel. Used for blur,
 *        shadows, morphology, transforms, and chaining filters together.
 *        Mirrors skity::ImageFilter.
 */
SKITY_C_DEFINE_HANDLE(skity_image_filter);

/**
 * @brief Create a Gaussian blur image filter.
 * @param sigma_x  horizontal blur sigma, in pixels; must be >= 0
 * @param sigma_y  vertical blur sigma, in pixels; must be >= 0
 * @return         filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image_filter skity_image_filter_create_blur(float sigma_x,
                                                              float sigma_y);

/**
 * @brief Create a morphology dilate (max) image filter.
 * @param radius_x  horizontal dilation radius, in pixels
 * @param radius_y  vertical dilation radius, in pixels
 * @return          filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image_filter skity_image_filter_create_dilate(float radius_x,
                                                                float radius_y);

/**
 * @brief Create a morphology erode (min) image filter.
 * @param radius_x  horizontal erosion radius, in pixels
 * @param radius_y  vertical erosion radius, in pixels
 * @return          filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image_filter skity_image_filter_create_erode(float radius_x,
                                                               float radius_y);

/**
 * @brief Create an image filter that transforms its input by @p matrix.
 * @param matrix  4x4 transform applied to the filtered result
 * @return        filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image_filter
skity_image_filter_create_matrix_transform(const skity_matrix* matrix);

/**
 * @brief Wrap a color filter as an image filter so it can be chained or
 *        attached where an image filter is expected.
 * @param filter  color filter to wrap
 * @return        image filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image_filter
skity_image_filter_create_from_color_filter(skity_color_filter filter);

/**
 * @brief Create a composed image filter: result = outer(inner(src)).
 * @param outer  filter applied last
 * @param inner  filter applied first
 * @return       composed filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image_filter skity_image_filter_create_compose(
    skity_image_filter outer, skity_image_filter inner);

/**
 * @brief Create a drop-shadow image filter.
 * @param dx       horizontal shadow offset, in pixels
 * @param dy       vertical shadow offset, in pixels
 * @param sigma_x  horizontal blur sigma of the shadow, in pixels
 * @param sigma_y  vertical blur sigma of the shadow, in pixels
 * @param color    shadow color, unpremultiplied ARGB packed as 0xAARRGGBB
 * @param input    source image filter to shadow, or NULL to use the source
 *                 primitive directly
 * @param crop     optional crop rectangle applied to the output, or NULL for
 *                 no crop
 * @return         filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image_filter skity_image_filter_create_drop_shadow(
    float dx, float dy, float sigma_x, float sigma_y, skity_color color,
    skity_image_filter input, const skity_rect* crop);

/**
 * @brief Wrap an image filter with a local matrix applied before the canvas
 *        matrix.
 * @param input   filter to wrap
 * @param matrix  local transform applied to @p input's sampling
 * @return        filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image_filter skity_image_filter_create_local_matrix(
    skity_image_filter input, const skity_matrix* matrix);

/** @brief Release the image filter handle and its underlying object. Safe on
 *         NULL. */
SKITY_C_API void skity_image_filter_destroy(skity_image_filter filter);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_IMAGE_FILTER_H
