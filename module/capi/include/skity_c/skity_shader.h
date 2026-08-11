// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SHADER_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SHADER_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_image.h>
#include <skity_c/skity_types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Shader: source color(s) for drawing. When a paint has a shader, its
 *        output replaces the paint's solid color. Gradients and image-based
 *        fills are shaders. Mirrors skity::Shader.
 */
SKITY_C_DEFINE_HANDLE(skity_shader);

/**
 * @brief Create a linear gradient shader between two points.
 * @param pts        start and end points; each entry uses only the x/y of the
 *                   Vec4
 * @param colors     array of @p count color stops
 * @param pos        array of @p count stop positions in [0, 1], or NULL to
 *                   distribute colors evenly
 * @param count      number of entries in @p colors (and @p pos when non-NULL);
 *                   must be >= 2
 * @param tile_mode  how colors are extrapolated outside the [0, 1] stop range
 * @param flags      non-zero to premultiply colors before interpolating
 * @return           shader handle, or NULL on invalid arguments
 */
SKITY_C_API skity_shader skity_shader_create_linear(
    const skity_point pts[2], const skity_color4f* colors, const float* pos,
    int32_t count, skity_tile_mode tile_mode, int32_t flags);

/**
 * @brief Create a radial gradient shader.
 * @param center     center of the gradient circle; only x/y of the Vec4 are
 *                   used
 * @param radius     radius of the gradient circle, in pixels; must be > 0
 * @param colors     array of @p count color stops
 * @param pos        array of @p count stop positions in [0, 1], or NULL to
 *                   distribute colors evenly
 * @param count      number of entries in @p colors; must be >= 2
 * @param tile_mode  how colors are extrapolated outside the [0, 1] stop range
 * @param flags      non-zero to premultiply colors before interpolating
 * @return           shader handle, or NULL on invalid arguments
 */
SKITY_C_API skity_shader skity_shader_create_radial(
    skity_point center, float radius, const skity_color4f* colors,
    const float* pos, int32_t count, skity_tile_mode tile_mode, int32_t flags);

/**
 * @brief Create a sweep (angular) gradient shader.
 * @param cx          x coordinate of the sweep center
 * @param cy          y coordinate of the sweep center
 * @param start_angle start of the angular range, corresponding to pos == 0
 * @param end_angle   end of the angular range, corresponding to pos == 1
 * @param colors      array of @p count color stops
 * @param pos         array of @p count stop positions in [0, 1], or NULL to
 *                    distribute colors evenly
 * @param count       number of entries in @p colors; must be >= 2
 * @param tile_mode   how colors are extrapolated outside the [0, 1] stop range
 * @param flags       non-zero to premultiply colors before interpolating
 * @return            shader handle, or NULL on invalid arguments
 */
SKITY_C_API skity_shader skity_shader_create_sweep(
    float cx, float cy, float start_angle, float end_angle,
    const skity_color4f* colors, const float* pos, int32_t count,
    skity_tile_mode tile_mode, int32_t flags);

/**
 * @brief Create a two-point conical gradient shader (a radial gradient between
 *        two circles), following the HTML canvas createRadialGradient spec.
 * @param start        center of the starting circle; only x/y of the Vec4 are
 *                     used
 * @param start_radius radius of the starting circle
 * @param end          center of the ending circle; only x/y of the Vec4 are
 *                     used
 * @param end_radius   radius of the ending circle
 * @param colors       array of @p count color stops
 * @param pos          array of @p count stop positions in [0, 1], or NULL to
 *                     distribute colors evenly
 * @param count        number of entries in @p colors; must be >= 2
 * @param tile_mode    how colors are extrapolated outside the [0, 1] stop range
 * @param flags        non-zero to premultiply colors before interpolating
 * @return             shader handle, or NULL on invalid arguments
 */
SKITY_C_API skity_shader skity_shader_create_two_point_conical(
    skity_point start, float start_radius, skity_point end, float end_radius,
    const skity_color4f* colors, const float* pos, int32_t count,
    skity_tile_mode tile_mode, int32_t flags);

/**
 * @brief Create an image shader that fills geometry with @p image's pixels.
 * @param image         source image
 * @param sampling      sampling options, or NULL for the defaults
 * @param x_tile_mode   tiling mode along the x axis
 * @param y_tile_mode   tiling mode along the y axis
 * @param local_matrix  optional local matrix applied before the canvas
 *                       matrix, or NULL for identity
 * @return              shader handle, or NULL on invalid arguments
 */
SKITY_C_API skity_shader skity_shader_create_image(
    skity_image image, const skity_sampling_options* sampling,
    skity_tile_mode x_tile_mode, skity_tile_mode y_tile_mode,
    const skity_matrix* local_matrix);

/**
 * @brief Replace the shader's local matrix.
 *
 * The local matrix is applied before the canvas matrix, mapping from "local"
 * shader coordinates into the geometry's coordinate space. Mirrors
 * skity::Shader::SetLocalMatrix.
 *
 * @param shader  shader handle
 * @param matrix  new local matrix, or NULL to reset to identity
 */
SKITY_C_API void skity_shader_set_local_matrix(skity_shader shader,
                                               const skity_matrix* matrix);

/**
 * @brief Read the shader's current local matrix.
 *
 * Mirrors skity::Shader::GetLocalMatrix.
 *
 * @param shader  shader handle
 * @param out     receives the 16 column-major floats; must point to a
 *                skity_matrix with room for 16 elements
 */
SKITY_C_API void skity_shader_get_local_matrix(skity_shader shader,
                                               skity_matrix* out);

/** @brief Release the shader handle and its underlying object. Safe on NULL. */
SKITY_C_API void skity_shader_destroy(skity_shader shader);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SHADER_H
