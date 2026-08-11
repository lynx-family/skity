// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_COLOR_FILTER_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_COLOR_FILTER_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ColorFilter: transforms the source color of each pixel during
 *        drawing. Filters are composable and can be wrapped as an image
 *        filter. Mirrors skity::ColorFilter.
 */
SKITY_C_DEFINE_HANDLE(skity_color_filter);

/**
 * @brief Create a color filter that blends each source pixel with a constant
 *        color using @p mode.
 * @param color  unpremultiplied ARGB, packed as 0xAARRGGBB
 * @param mode   blend mode applied per pixel
 * @return       filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_color_filter
skity_color_filter_create_blend(skity_color color, skity_blend_mode mode);

/**
 * @brief Create a color filter that applies @p outer after @p inner:
 *        result = outer(inner(src)).
 * @param outer  filter applied last
 * @param inner  filter applied first
 * @return       composed filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_color_filter skity_color_filter_create_compose(
    skity_color_filter outer, skity_color_filter inner);

/**
 * @brief Create a color filter that applies a 4x5 color matrix to each pixel.
 * @param row_major  20 floats, 4 rows x 5 columns (RGBA + bias)
 * @return           filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_color_filter
skity_color_filter_create_matrix(const float* row_major);

/** @brief Create a color filter that converts sRGB-encoded pixels to a linear
 *         gamma. */
SKITY_C_API skity_color_filter skity_color_filter_create_linear_to_srgb(void);

/** @brief Create a color filter that converts linear-gamma pixels to sRGB
 *         encoding. */
SKITY_C_API skity_color_filter skity_color_filter_create_srgb_to_linear(void);

/** @brief Release the color filter handle and its underlying object. Safe on
 *         NULL. */
SKITY_C_API void skity_color_filter_destroy(skity_color_filter filter);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_COLOR_FILTER_H
