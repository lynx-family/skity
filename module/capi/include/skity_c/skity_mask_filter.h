// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_MASK_FILTER_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_MASK_FILTER_H

#include <skity_c/skity_base.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MaskFilter: modifies the alpha mask of a drawing primitive before it
 *        is rasterized. Currently used to apply Gaussian blur to edges and
 *        alpha. Mirrors skity::MaskFilter.
 */
SKITY_C_DEFINE_HANDLE(skity_mask_filter);

/** @brief Blur style, controlling what is drawn inside vs. outside the
 *         geometry's alpha mask. Values aligned with skity::BlurStyle
 *         (starts at 1). */
typedef enum {
  SKITY_BLUR_STYLE_NORMAL = 1, /**< fuzzy inside and outside */
  SKITY_BLUR_STYLE_SOLID = 2,  /**< solid inside, fuzzy outside */
  SKITY_BLUR_STYLE_OUTER = 3,  /**< nothing inside, fuzzy outside */
  SKITY_BLUR_STYLE_INNER = 4,  /**< fuzzy inside, nothing outside */
} skity_blur_style;

/**
 * @brief Create a Gaussian blur mask filter.
 * @param style   which side(s) of the mask are blurred
 * @param radius  Gaussian blur radius, in pixels; must be > 0
 * @return        mask filter handle, or NULL on invalid arguments
 */
SKITY_C_API skity_mask_filter
skity_mask_filter_create_blur(skity_blur_style style, float radius);

/** @brief Release the mask filter handle and its underlying object. Safe on
 *         NULL. */
SKITY_C_API void skity_mask_filter_destroy(skity_mask_filter filter);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_MASK_FILTER_H
