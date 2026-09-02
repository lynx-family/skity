// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Path describes a piece of vector geometry as a sequence of verbs
 *        (move / line / quad / conic / cubic / close) and their control points.
 *        It is the geometry source for drawing and clipping, and the operand
 *        for boolean path operations.
 */
SKITY_C_DEFINE_HANDLE(skity_path);

/** @brief Contour winding direction. Values aligned with
 *         skity::Path::Direction. */
typedef enum {
  SKITY_PATH_DIRECTION_CW = 0, /**< clockwise winding */
  SKITY_PATH_DIRECTION_CCW,    /**< counter-clockwise winding */
} skity_path_direction;

/** @brief Fill rule used to decide whether a point is inside the path.
 *         Values aligned with skity::Path::PathFillType. */
typedef enum {
  /** non-zero sum of signed edge crossings counts as inside */
  SKITY_PATH_FILL_TYPE_WINDING = 0,
  SKITY_PATH_FILL_TYPE_EVEN_ODD, /**< odd number of edge crossings is inside */
} skity_path_fill_type;

/** @brief Choice of arc size for SVG-style elliptical arcs. Values aligned
 *         with skity::Path::ArcSize (kSmall = 0, kLarge = 1). */
typedef enum {
  SKITY_ARC_SIZE_SMALL = 0, /**< sweep the smaller of the two candidate arcs */
  SKITY_ARC_SIZE_LARGE = 1, /**< sweep the larger of the two candidate arcs */
} skity_arc_size;

/** @brief Path verb kinds, in storage order. Values aligned with
 *         skity::Path::Verb. */
typedef enum {
  SKITY_PATH_VERB_MOVE = 0, /**< begin a contour; 1 point */
  SKITY_PATH_VERB_LINE,     /**< line segment; 2 points */
  SKITY_PATH_VERB_QUAD,     /**< quadratic curve; 3 points */
  SKITY_PATH_VERB_CONIC,    /**< conic curve; 3 points + weight */
  SKITY_PATH_VERB_CUBIC,    /**< cubic curve; 4 points */
  SKITY_PATH_VERB_CLOSE,    /**< close the contour; 1 point */
  SKITY_PATH_VERB_DONE,     /**< no more verbs */
} skity_path_verb;

/** @brief Convexity classification. Values aligned with
 *         skity::Path::ConvexityType. */
typedef enum {
  SKITY_CONVEXITY_UNKNOWN = 0, /**< not yet computed */
  SKITY_CONVEXITY_CONVEX,      /**< all interior angles <= 180 degrees */
  SKITY_CONVEXITY_CONCAVE,     /**< at least one interior angle > 180 degrees */
} skity_convexity_type;

/** @brief How an added path is appended to the existing contours. Values
 *         aligned with skity::Path::AddMode. */
typedef enum {
  SKITY_PATH_ADD_MODE_APPEND =
      0,                      /**< append src to the destination unaltered */
  SKITY_PATH_ADD_MODE_EXTEND, /**< add a line to src if the prior contour is
                                   not closed */
} skity_path_add_mode;

/** @brief Create an empty path. */
SKITY_C_API skity_path skity_path_create(void);

/** @brief Release the path handle and its underlying object. Safe on NULL. */
SKITY_C_API void skity_path_destroy(skity_path path);

/** @brief Create a new path that is a copy of @p src. */
SKITY_C_API skity_path skity_path_clone(skity_path src);

/** @brief Return whether the two paths hold equal verbs, points, and weights.
 */
SKITY_C_API uint32_t skity_path_is_equal(skity_path a, skity_path b);

/** @brief Clear all verbs and points, leaving the path empty (fill type is
 *         retained). */
SKITY_C_API void skity_path_reset(skity_path path);

/** @brief Set the rule used to test whether a point is inside the path. */
SKITY_C_API void skity_path_set_fill_type(skity_path path,
                                          skity_path_fill_type type);

/**
 * @brief Begin a new contour at (x, y). If a contour is already in progress it
 *        is closed logically (no Close verb is emitted) and a new one starts.
 */
SKITY_C_API void skity_path_move_to(skity_path path, float x, float y);

/** @brief Append a straight line from the last point to (x, y). */
SKITY_C_API void skity_path_line_to(skity_path path, float x, float y);

/**
 * @brief Append a quadratic Bezier from the last point using one control point.
 * @param x1, y1  the quadratic control point
 * @param x2, y2  the end point of the curve
 */
SKITY_C_API void skity_path_quad_to(skity_path path, float x1, float y1,
                                    float x2, float y2);

/**
 * @brief Append a rational quadratic (conic) from the last point.
 * @param x1, y1  the control point
 * @param x2, y2  the end point of the curve
 * @param weight  conic weight; 1.0 yields a quadratic, < 1 flattens toward the
 *                chord, > 1 bulges toward the control point
 */
SKITY_C_API void skity_path_conic_to(skity_path path, float x1, float y1,
                                     float x2, float y2, float weight);

/**
 * @brief Append a cubic Bezier from the last point using two control points.
 * @param x1, y1  the first control point
 * @param x2, y2  the second control point
 * @param x3, y3  the end point of the curve
 */
SKITY_C_API void skity_path_cubic_to(skity_path path, float x1, float y1,
                                     float x2, float y2, float x3, float y3);

/**
 * @brief Append a circular arc tangent to the segment from the last point
 *        towards (x1, y1) and ending tangent to the segment towards (x2, y2).
 *        A connecting line is appended first so the contour stays continuous.
 * @param x1, y1  intermediate point the incoming tangent points at
 * @param x2, y2  point the outgoing tangent is taken from
 * @param radius   radius of the appended arc
 */
SKITY_C_API void skity_path_arc_to(skity_path path, float x1, float y1,
                                   float x2, float y2, float radius);

/** @brief Close the current contour by appending a line back to its start. */
SKITY_C_API void skity_path_close(skity_path path);

/**
 * @brief Append a circle centered at (x, y) with the given radius as a new
 *        contour. Has no effect when radius <= 0.
 * @param dir  winding direction of the new contour
 */
SKITY_C_API void skity_path_add_circle(skity_path path, float x, float y,
                                       float radius, skity_path_direction dir);

/**
 * @brief Append an upright ellipse bounded by @p oval as a new contour.
 * @param oval  bounding rect of the ellipse
 * @param dir   winding direction of the new contour
 */
SKITY_C_API void skity_path_add_oval(skity_path path, const skity_rect* oval,
                                     skity_path_direction dir);

/**
 * @brief Append @p rect as a closed contour (move + three lines + close).
 * @param dir  winding direction of the new contour
 */
SKITY_C_API void skity_path_add_rect(skity_path path, const skity_rect* rect,
                                     skity_path_direction dir);

/**
 * @brief Append @p rect with corner radii (rx, ry) as a new contour.
 * @param rx, ry  x and y radii of every corner
 * @param dir     winding direction of the new contour
 */
SKITY_C_API void skity_path_add_round_rect(skity_path path,
                                           const skity_rect* rect, float rx,
                                           float ry, skity_path_direction dir);

/** @brief Apply @p matrix to every point of the path in place. */
SKITY_C_API void skity_path_transform(skity_path path,
                                      const skity_matrix* matrix);

/** @brief Write the bounding box of all points into @p out_bounds. */
SKITY_C_API void skity_path_get_bounds(skity_path path, skity_rect* out_bounds);

/**
 * @brief Append @p src to @p path, offset by (dx, dy).
 */
SKITY_C_API void skity_path_add_path(skity_path path, skity_path src, float dx,
                                     float dy);

/**
 * @brief Append @p rect with uniform corner radii (rx, ry) as a new contour.
 *        Same shape as skity_path_add_round_rect; kept for naming parity with
 *        the C++ API.
 * @param dir  winding direction of the new contour
 */
SKITY_C_API void skity_path_add_rrect(skity_path path, const skity_rect* rect,
                                      float rx, float ry,
                                      skity_path_direction dir);

/** @brief Return 1 if the point (x, y) lies inside the path under the current
 *         fill type, 0 otherwise. */
SKITY_C_API uint32_t skity_path_contains(skity_path path, float x, float y);

/** @brief Return the fill rule currently in effect. */
SKITY_C_API skity_path_fill_type skity_path_get_fill_type(skity_path path);

/** @brief Append @p src to @p path after transforming it by @p matrix
 *         (append mode — no connecting line is inserted). */
SKITY_C_API void skity_path_add_path_matrix(skity_path path, skity_path src,
                                            const skity_matrix* matrix);

/**
 * @brief Append @p src to @p path under @p mode. With
 *        SKITY_PATH_ADD_MODE_EXTEND, a line is added first when the prior
 *        contour is not closed.
 */
SKITY_C_API void skity_path_add_path_with_mode(skity_path path, skity_path src,
                                               skity_path_add_mode mode);

/**
 * @brief Append @p src to @p path under @p mode after transforming it by
 *        @p matrix.
 */
SKITY_C_API void skity_path_add_path_matrix_with_mode(
    skity_path path, skity_path src, const skity_matrix* matrix,
    skity_path_add_mode mode);

/** @brief Append the contours of @p src in reverse order to @p path. */
SKITY_C_API void skity_path_reverse_add_path(skity_path path, skity_path src);

/** @brief Return the number of verbs stored in the path. */
SKITY_C_API uint32_t skity_path_count_verbs(skity_path path);

/** @brief Return the number of control points stored in the path. */
SKITY_C_API uint32_t skity_path_count_points(skity_path path);

/**
 * @brief Write the point at @p index into @p out as a Vec4.
 * @return 1 on success, 0 if @p index is out of range.
 */
SKITY_C_API uint32_t skity_path_get_point(skity_path path, int32_t index,
                                          skity_vec4* out);

/**
 * @brief Return the verb kind at @p index. Pair with
 *        skity_path_count_verbs / skity_path_count_points /
 *        skity_path_get_point to walk the path by index.
 *
 *        Conic verbs carry a weight that the index-based walk cannot fetch;
 *        it is only available through the C++ Path::Iter.
 *
 * @return the verb kind, or SKITY_PATH_VERB_DONE if @p index is out of range.
 */
SKITY_C_API skity_path_verb skity_path_get_verb(skity_path path, int32_t index);

/**
 * @brief Write the weight of the @p index -th conic segment into @p out.
 *        The index counts conic verbs only, in storage order.
 * @return 1 on success, 0 if @p index is out of range.
 */
SKITY_C_API uint32_t skity_path_get_conic_weight(skity_path path, int32_t index,
                                                 float* out);

/**
 * @brief Test whether the path is equivalent to a single rect when filled.
 * @param out_rect      if non-NULL and the test passes, receives the rect
 * @param out_is_closed if non-NULL and the test passes, set to 1 when the
 *                      contour is closed, 0 otherwise
 * @return 1 if the path is a rect, 0 otherwise.
 */
SKITY_C_API uint32_t skity_path_is_rect(skity_path path, skity_rect* out_rect,
                                        uint32_t* out_is_closed);

/**
 * @brief Return the convexity classification. SKITY_CONVEXITY_UNKNOWN is
 *        computed on demand, so the first call may take time.
 */
SKITY_C_API skity_convexity_type skity_path_get_convexity_type(skity_path path);

/** @brief Override the cached convexity classification. */
SKITY_C_API void skity_path_set_convexity_type(skity_path path,
                                               skity_convexity_type type);

/**
 * @brief Return a bit mask of the segment kinds present in the path:
 *        bit 0 line, bit 1 quad, bit 2 conic, bit 3 cubic. Cached, very fast.
 */
SKITY_C_API uint32_t skity_path_get_segment_masks(skity_path path);

/** @brief Return whether all points of the path are finite. */
SKITY_C_API uint32_t skity_path_is_finite(skity_path path);

/** @brief Return whether the path holds no verbs. */
SKITY_C_API uint32_t skity_path_is_empty(skity_path path);

/**
 * @brief Test whether the path is equivalent to a single line segment.
 * @param out_pts  if non-NULL and the test passes, receives the two endpoints
 * @return 1 if the path is a line, 0 otherwise.
 */
SKITY_C_API uint32_t skity_path_is_line(skity_path path, skity_vec4* out_pts);

/** @brief Write the point of the most recent Move verb into @p out. The
 *         result of an empty path is unspecified. */
SKITY_C_API void skity_path_get_last_move_pt(skity_path path, skity_vec4* out);

/**
 * @brief Append an elliptical arc that lies on the ellipse bounded by @p oval.
 *        Angles are in degrees; zero degrees aligns with the positive x-axis
 *        and a positive sweepAngle extends the arc clockwise.
 * @param oval           bounds of the containing ellipse
 * @param start_angle    starting angle in degrees
 * @param sweep_angle    sweep in degrees; positive is clockwise, modulo 360
 * @param force_move_to  non-zero to begin a new contour with this arc; zero to
 *                       keep the contour continuous (a connecting line is
 *                       appended when the last point differs from the arc
 * start)
 */
SKITY_C_API void skity_path_arc_to_oval(skity_path path, const skity_rect* oval,
                                        float start_angle, float sweep_angle,
                                        uint32_t force_move_to);

/**
 * @brief Append an SVG-style elliptical arc from the last point to (x, y).
 * @param rx, ry         radii of the ellipse
 * @param x_axis_rotate  x-axis rotation of the ellipse in degrees
 * @param large_arc      choose the larger (SKITY_ARC_SIZE_LARGE) or smaller
 *                       candidate arc
 * @param sweep          winding direction of the arc (CW or CCW)
 * @param x, y           end point of the arc
 */
SKITY_C_API void skity_path_arc_to_svg(skity_path path, float rx, float ry,
                                       float x_axis_rotate,
                                       skity_arc_size large_arc,
                                       skity_path_direction sweep, float x,
                                       float y);

/**
 * @brief Append @p rect with per-corner radii as a new contour, allowing each
 *        corner to use different x and y radii.
 * @param rect   bounds of the rounded rectangle
 * @param radii  8 floats, laid out as (x, y) pairs in corner order:
 *               top-left, top-right, bottom-right, bottom-left (the same order
 *               used by skity::RRect::Corner). May be NULL, in which case the
 *               call is a no-op.
 * @param dir    winding direction of the new contour
 */
SKITY_C_API void skity_path_add_round_rect_radii(skity_path path,
                                                 const skity_rect* rect,
                                                 const float* radii,
                                                 skity_path_direction dir);

/**
 * @brief Write the last point of the path into @p out.
 * @return 1 if the path had a last point (and @p out is non-NULL); 0 otherwise.
 *         On failure @p out is left untouched.
 */
SKITY_C_API uint32_t skity_path_get_last_pt(skity_path path, skity_point* out);

/**
 * @brief Replace the last point of the path with (x, y). If the path is empty a
 *        MoveTo is appended instead.
 */
SKITY_C_API void skity_path_set_last_pt(skity_path path, float x, float y);

/**
 * @brief Return a new owning handle holding a transformed copy of @p path; the
 *        original path is not modified. Returns NULL on a null/wrong-type
 *        handle or a NULL @p matrix. The caller owns the returned handle and
 *        must release it with skity_path_destroy.
 */
SKITY_C_API skity_path skity_path_copy_with_matrix(skity_path path,
                                                   const skity_matrix* matrix);

/**
 * @brief Return a new owning handle holding a uniformly scaled copy of @p path;
 *        the original path is not modified. Returns NULL on a null handle. The
 *        caller owns the returned handle and must release it with
 *        skity_path_destroy.
 */
SKITY_C_API skity_path skity_path_copy_with_scale(skity_path path, float scale);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PATH_H
