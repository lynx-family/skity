// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_BUILDER_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_BUILDER_H

#include <skity/c/skity_path.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mutable path construction with independent Path results.
 *        Handles are owned by their caller; copying a handle does not clone it.
 *        NULL/wrong-type handles make writes no-ops and queries fail.
 */
SKITY_C_DEFINE_HANDLE(skity_path_builder);

/** @brief Creates an empty winding builder. */
SKITY_C_API skity_path_builder skity_path_builder_create(void);
/** @brief Releases the builder; already produced Paths remain valid. Safe on
 * NULL. */
SKITY_C_API void skity_path_builder_destroy(skity_path_builder builder);
/** @brief Copies source for editing; source remains unchanged. */
SKITY_C_API skity_path_builder
skity_path_builder_create_from_path(skity_path source);
/**
 * @brief Transfers an owning source's geometry into a new builder.
 *        The source handle remains valid and must still be destroyed; its
 *        Path becomes empty winding. Borrowed sources are rejected.
 * @return new builder, or NULL without changing source on a reported failure
 */
SKITY_C_API skity_path_builder
skity_path_builder_create_from_path_move(skity_path source);

/** @brief Returns an independent Path copy, retaining builder content/capacity.
 */
SKITY_C_API skity_path skity_path_builder_snapshot(skity_path_builder builder);
/**
 * @brief Transfers geometry to a new owning Path and empties builder to
 * winding. Geometry is not copied; a new result object/handle is still
 * allocated.
 * @return Path to release with skity_path_destroy, or NULL without consuming
 * builder
 */
SKITY_C_API skity_path skity_path_builder_detach(skity_path_builder builder);
/**
 * @brief Replaces an existing owning Path, keeping its object/handle identity.
 *        Its old geometry capacity is reused by the now-empty winding builder.
 *        This mutation invalidates destination iterators.
 * @return 1 on success; 0 on NULL/wrong-type/borrowed destination, changing
 * neither
 */
SKITY_C_API uint32_t skity_path_builder_detach_to(skity_path_builder builder,
                                                  skity_path destination);

SKITY_C_API void skity_path_builder_move_to(skity_path_builder builder, float x,
                                            float y);

SKITY_C_API void skity_path_builder_line_to(skity_path_builder builder, float x,
                                            float y);

SKITY_C_API void skity_path_builder_quad_to(skity_path_builder builder,
                                            float x1, float y1, float x2,
                                            float y2);

SKITY_C_API void skity_path_builder_conic_to(skity_path_builder builder,
                                             float x1, float y1, float x2,
                                             float y2, float weight);

SKITY_C_API void skity_path_builder_cubic_to(skity_path_builder builder,
                                             float x1, float y1, float x2,
                                             float y2, float x3, float y3);

SKITY_C_API void skity_path_builder_arc_to(skity_path_builder builder, float x1,
                                           float y1, float x2, float y2,
                                           float radius);

SKITY_C_API void skity_path_builder_arc_to_oval(skity_path_builder builder,
                                                const skity_rect* oval,
                                                float start_angle,
                                                float sweep_angle,
                                                uint32_t force_move_to);

SKITY_C_API void skity_path_builder_arc_to_svg(
    skity_path_builder builder, float rx, float ry, float rotation,
    skity_arc_size large_arc, skity_path_direction sweep, float x, float y);

SKITY_C_API void skity_path_builder_close(skity_path_builder builder);

/** @brief Clears geometry, restores winding fill, and retains array capacity.
 */
SKITY_C_API void skity_path_builder_reset(skity_path_builder builder);

/** @brief Reserves total capacities without changing geometry or shrinking
 * arrays. */
SKITY_C_API void skity_path_builder_reserve(skity_path_builder builder,
                                            size_t point_count,
                                            size_t verb_count,
                                            size_t conic_weight_count);

SKITY_C_API void skity_path_builder_set_fill_type(skity_path_builder builder,
                                                  skity_path_fill_type type);

SKITY_C_API void skity_path_builder_set_last_pt(skity_path_builder builder,
                                                float x, float y);

SKITY_C_API void skity_path_builder_add_circle(skity_path_builder builder,
                                               float x, float y, float radius,
                                               skity_path_direction dir);

SKITY_C_API void skity_path_builder_add_rect(skity_path_builder builder,
                                             const skity_rect* rect,
                                             skity_path_direction dir);

/** @brief Uses an explicit start index, wrapping modulo 4. */
SKITY_C_API void skity_path_builder_add_rect_start(skity_path_builder builder,
                                                   const skity_rect* rect,
                                                   skity_path_direction dir,
                                                   uint32_t start_index);

SKITY_C_API void skity_path_builder_add_oval(skity_path_builder builder,
                                             const skity_rect* oval,
                                             skity_path_direction dir);

/** @brief Uses an explicit start index, wrapping modulo 4. */
SKITY_C_API void skity_path_builder_add_oval_start(skity_path_builder builder,
                                                   const skity_rect* oval,
                                                   skity_path_direction dir,
                                                   uint32_t start_index);

SKITY_C_API void skity_path_builder_add_round_rect(skity_path_builder builder,
                                                   const skity_rect* rect,
                                                   float rx, float ry,
                                                   skity_path_direction dir);

/** @brief Uses eight floats: x/y radii in top-left, top-right, bottom-right,
 * bottom-left order. */
SKITY_C_API void skity_path_builder_add_round_rect_radii(
    skity_path_builder builder, const skity_rect* rect, const float radii[8],
    skity_path_direction dir);

/** @brief Uses four x/y radii in top-left, top-right, bottom-right, bottom-left
 * order. */
SKITY_C_API void skity_path_builder_add_rrect(skity_path_builder builder,
                                              const skity_rect* rect,
                                              const skity_vec2 radii[4],
                                              skity_path_direction dir);

/** @brief Uses an explicit start index, wrapping modulo 8. */
SKITY_C_API void skity_path_builder_add_rrect_start(skity_path_builder builder,
                                                    const skity_rect* rect,
                                                    const skity_vec2 radii[4],
                                                    skity_path_direction dir,
                                                    uint32_t start_index);

SKITY_C_API void skity_path_builder_add_path(skity_path_builder builder,
                                             skity_path source, float dx,
                                             float dy,
                                             skity_path_add_mode mode);
SKITY_C_API void skity_path_builder_add_path_matrix(skity_path_builder builder,
                                                    skity_path source,
                                                    const skity_matrix* matrix,
                                                    skity_path_add_mode mode);
SKITY_C_API void skity_path_builder_reverse_add_path(skity_path_builder builder,
                                                     skity_path source);
/** @brief Appends the source's last contour backwards, omitting its last point.
 */
SKITY_C_API void skity_path_builder_reverse_path_to(skity_path_builder builder,
                                                    skity_path source);
/** @brief Like reverse_path_to, reading another builder; self-append is
 * supported. */
SKITY_C_API void skity_path_builder_reverse_builder_to(
    skity_path_builder builder, skity_path_builder source);
SKITY_C_API void skity_path_builder_swap(skity_path_builder builder,
                                         skity_path_builder other);

SKITY_C_API uint32_t skity_path_builder_is_empty(skity_path_builder builder);
SKITY_C_API size_t skity_path_builder_count_points(skity_path_builder builder);
SKITY_C_API size_t skity_path_builder_count_verbs(skity_path_builder builder);
SKITY_C_API skity_path_fill_type
skity_path_builder_get_fill_type(skity_path_builder builder);
/** @brief Writes x/y; returns 0 without writing on invalid input or
 * out-of-range index. */
SKITY_C_API uint32_t skity_path_builder_get_point(skity_path_builder builder,
                                                  int32_t index,
                                                  skity_vec2* out);
/** @brief Writes the last x/y point; returns 0 without writing when absent or
 * invalid. */
SKITY_C_API uint32_t skity_path_builder_get_last_pt(skity_path_builder builder,
                                                    skity_vec2* out);
/** @brief Writes the last move's x/y; returns 0 without writing when absent or
 * invalid. */
SKITY_C_API uint32_t skity_path_builder_get_last_move_pt(
    skity_path_builder builder, skity_vec2* out);
/** @brief Writes the point bounds; returns 0 without writing on invalid input.
 */
SKITY_C_API uint32_t skity_path_builder_get_bounds(skity_path_builder builder,
                                                   skity_rect* out);

#ifdef __cplusplus
}
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_BUILDER_H
