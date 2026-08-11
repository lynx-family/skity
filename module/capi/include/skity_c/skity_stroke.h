// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_STROKE_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_STROKE_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_paint.h>
#include <skity_c/skity_path.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Stroke @p src using the stroke parameters carried by @p paint (width,
 *        cap, join, miter) and append the resulting outline to @p dst. @p paint
 *        must be set to a stroke style for a meaningful result.
 * @param paint  source of stroke parameters (style, width, cap, join, miter)
 * @param src    input path to outline
 * @param dst    destination path the outline is appended to
 */
SKITY_C_API void skity_stroke_stroke_path(skity_paint paint, skity_path src,
                                          skity_path dst);

/**
 * @brief Convert @p src into a quadratic / cubic outline per @p paint's stroke
 *        settings and append it to @p dst.
 * @param paint       source of stroke parameters
 * @param src         input path to outline
 * @param dst         destination path the outline is appended to
 * @param keep_cubic  non-zero to preserve cubic segments, 0 to flatten them
 */
SKITY_C_API void skity_stroke_quad_path(skity_paint paint, skity_path src,
                                        skity_path dst, uint32_t keep_cubic);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_STROKE_H
