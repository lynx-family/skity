// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_EFFECT_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_EFFECT_H

#include <skity_c/skity_base.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PathEffect: transforms a path's geometry before rasterization
 *        (e.g. dashing, jittering). Attached to a paint. Mirrors
 *        skity::PathEffect.
 */
SKITY_C_DEFINE_HANDLE(skity_path_effect);

/**
 * @brief Create a discrete path effect that chops the path into segments of
 *        length @p seg_length and randomly displaces each segment's vertices.
 * @param seg_length   length of each segment; must be > 0
 * @param dev          maximum displacement applied to each vertex; must be
 *                     >= 0
 * @param seed_assist  seed for the pseudo-random displacements; 0 makes the
 *                     result deterministic
 * @return             path effect handle, or NULL on invalid arguments
 */
SKITY_C_API skity_path_effect skity_path_effect_create_discrete(
    float seg_length, float dev, uint32_t seed_assist);

/**
 * @brief Create a dash path effect that strokes a path as a series of dashes.
 * @param intervals  array of on/off lengths; must have @p count entries
 *                   (an even number)
 * @param count      number of entries in @p intervals; must be even
 * @param phase      offset into the dash pattern, in pixels
 * @return           path effect handle, or NULL on invalid arguments
 */
SKITY_C_API skity_path_effect skity_path_effect_create_dash(
    const float* intervals, int32_t count, float phase);

/** @brief Release the path effect handle and its underlying object. Safe on
 *         NULL. */
SKITY_C_API void skity_path_effect_destroy(skity_path_effect effect);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_EFFECT_H
