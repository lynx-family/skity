// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_MEASURE_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_MEASURE_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_path.h>
#include <skity_c/skity_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PathMeasure samples a path by arc length: total length, position and
 *        tangent at a distance, and sub-paths between two distances. A path
 *        with multiple contours is measured one contour at a time — advance
 *        with skity_path_measure_next_contour.
 */
SKITY_C_DEFINE_HANDLE(skity_path_measure);

/**
 * @brief Create a measure over @p path.
 * @param path          the path to measure; may be NULL to create an empty
 *                      measure (bind a path later with set_path)
 * @param force_closed  non-zero to synthesize a close when the contour is open
 * @param res_scale     resolution scale; values > 1 increase measuring
 *                      precision (1.0 is normal)
 */
SKITY_C_API skity_path_measure skity_path_measure_create(skity_path path,
                                                         uint32_t force_closed,
                                                         float res_scale);

/** @brief Release the measure handle and its underlying object. Safe on NULL.
 */
SKITY_C_API void skity_path_measure_destroy(skity_path_measure measure);

/**
 * @brief Re-bind this measure to @p path, replacing any previously bound path.
 * @param path          the new path to measure; NULL detaches
 * @param force_closed  non-zero to synthesize a close when the contour is open
 * @return 1 on success, 0 if @p measure is a wrong-type handle.
 */
SKITY_C_API uint32_t skity_path_measure_set_path(skity_path_measure measure,
                                                 skity_path path,
                                                 uint32_t force_closed);

/** @brief Return the total length of the current contour, or 0 if no path is
 *         bound. */
SKITY_C_API float skity_path_measure_get_length(skity_path_measure measure);

/**
 * @brief Sample the position and unit tangent at @p distance along the current
 *        contour.
 *
 * @p distance is clamped to [0, length]. Both outputs are skity_vec4
 * (skity::Point / skity::Vector are Vec4) and either may be NULL.
 *
 * @return 1 on success, 0 if no path is bound or the contour is zero-length.
 */
SKITY_C_API uint32_t skity_path_measure_get_pos_tan(skity_path_measure measure,
                                                    float distance,
                                                    skity_vec4* out_position,
                                                    skity_vec4* out_tangent);

/**
 * @brief Append the sub-path of the current contour between @p start_d and
 *        @p stop_d to @p dst.
 * @param start_d           start distance along the contour
 * @param stop_d            stop distance along the contour
 * @param dst               destination path that receives the segment
 * @param start_with_move_to  non-zero to begin the segment with a MoveTo,
 *                          0 to continue the current contour
 * @return 1 on success, 0 if the segment is zero-length or @p start_d >
 *         @p stop_d.
 */
SKITY_C_API uint32_t skity_path_measure_get_segment(
    skity_path_measure measure, float start_d, float stop_d, skity_path dst,
    uint32_t start_with_move_to);

/** @brief Return 1 if the current contour is closed, 0 otherwise. */
SKITY_C_API uint32_t skity_path_measure_is_closed(skity_path_measure measure);

/** @brief Advance to the next contour. Return 1 if one exists, 0 at the end. */
SKITY_C_API uint32_t
skity_path_measure_next_contour(skity_path_measure measure);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_MEASURE_H
