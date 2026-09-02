// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CANVAS_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CANVAS_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_font.h>
#include <skity_c/skity_image.h>
#include <skity_c/skity_paint.h>
#include <skity_c/skity_path.h>
#include <skity_c/skity_surface.h>
#include <skity_c/skity_text.h>
#include <skity_c/skity_types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Clip operation. Values aligned with skity::Canvas::ClipOp. */
typedef enum {
  SKITY_CLIP_OP_DIFFERENCE = 0, /**< subtract the shape from the current clip */
  SKITY_CLIP_OP_INTERSECT, /**< intersect the shape with the current clip */
} skity_clip_op;

/**
 * @brief Save the current matrix and clip on an internal stack so later
 *        changes can be rolled back with skity_canvas_restore. This is the
 *        entry point of the save/restore and matrix/clip state group.
 *
 * @return the depth of the save stack after this save
 */
SKITY_C_API int32_t skity_canvas_save(skity_canvas canvas);

/** @brief Roll back changes to the matrix and clip made since the most recent
 *         skity_canvas_save. */
SKITY_C_API void skity_canvas_restore(skity_canvas canvas);

/**
 * @brief Restore state to the matrix and clip captured when
 *        skity_canvas_save returned @p save_count. Does nothing if
 *        @p save_count is greater than the current stack depth.
 * @param save_count  value previously returned by skity_canvas_save
 */
SKITY_C_API void skity_canvas_restore_to_count(skity_canvas canvas,
                                               int32_t save_count);

/** @brief Return the current depth of the save stack. */
SKITY_C_API int32_t skity_canvas_get_save_count(skity_canvas canvas);

/**
 * @brief Translate the current matrix by @p dx along the x-axis and @p dy
 *        along the y-axis.
 * @param dx  distance to translate on the x-axis
 * @param dy  distance to translate on the y-axis
 */
SKITY_C_API void skity_canvas_translate(skity_canvas canvas, float dx,
                                        float dy);

/**
 * @brief Scale the current matrix by @p sx on the x-axis and @p sy on the
 *        y-axis.
 * @param sx  amount to scale on the x-axis
 * @param sy  amount to scale on the y-axis
 */
SKITY_C_API void skity_canvas_scale(skity_canvas canvas, float sx, float sy);

/**
 * @brief Rotate the current matrix by @p degrees. Positive values rotate
 *        clockwise.
 * @param degrees  amount to rotate, in degrees
 */
SKITY_C_API void skity_canvas_rotate(skity_canvas canvas, float degrees);

/**
 * @brief Rotate the current matrix by @p degrees about the point (@p px, @p
 * py).
 * @param degrees  amount to rotate, in degrees
 * @param px       x-axis value of the point to rotate about
 * @param py       y-axis value of the point to rotate about
 */
SKITY_C_API void skity_canvas_rotate_deg(skity_canvas canvas, float degrees,
                                         float px, float py);

/**
 * @brief Skew the current matrix by @p sx on the x-axis and @p sy on the
 *        y-axis.
 * @param sx  amount to skew on the x-axis
 * @param sy  amount to skew on the y-axis
 */
SKITY_C_API void skity_canvas_skew(skity_canvas canvas, float sx, float sy);

/**
 * @brief Replace the current matrix with @p matrix premultiplied with the
 *        existing matrix.
 * @param matrix  matrix to premultiply with the existing matrix
 */
SKITY_C_API void skity_canvas_concat(skity_canvas canvas,
                                     const skity_matrix* matrix);

/**
 * @brief Replace the current matrix with @p matrix.
 * @param matrix  matrix to replace the existing matrix
 */
SKITY_C_API void skity_canvas_set_matrix(skity_canvas canvas,
                                         const skity_matrix* matrix);

/** @brief Reset the current matrix to the identity matrix. */
SKITY_C_API void skity_canvas_reset_matrix(skity_canvas canvas);

/**
 * @brief Combine @p rect with the current clip using @p op, replacing the
 *        clip with the result.
 * @param rect  rectangle to combine with the clip
 * @param op    clip operation to apply
 */
SKITY_C_API void skity_canvas_clip_rect(skity_canvas canvas,
                                        const skity_rect* rect,
                                        skity_clip_op op);

/**
 * @brief Combine @p path with the current clip using @p op, replacing the
 *        clip with the result.
 * @param path  path to combine with the clip
 * @param op    clip operation to apply
 */
SKITY_C_API void skity_canvas_clip_path(skity_canvas canvas, skity_path path,
                                        skity_clip_op op);

/**
 * @brief Fill the current clip with a solid @p color combined via @p mode.
 *        This begins the drawing-primitive group; the draw_xxx functions
 *        below render geometry, images, and text using the current matrix,
 *        clip, and the supplied paint.
 * @param color  unpremultiplied ARGB, packed as 0xAARRGGBB
 * @param mode   blend mode used to combine the source color with the
 * destination
 */
SKITY_C_API void skity_canvas_draw_color(skity_canvas canvas, skity_color color,
                                         skity_blend_mode mode);

/**
 * @brief Fill the entire clip with @p color using @p mode.
 * @param color  unpremultiplied RGBA, one float per channel
 * @param mode   blend mode used to combine the source color with the
 * destination
 */
SKITY_C_API void skity_canvas_draw_color4f(skity_canvas canvas,
                                           skity_color4f color,
                                           skity_blend_mode mode);

/**
 * @brief Fill the entire clip using @p paint. The paint's color, shader,
 *        color filter, image filter, and blend mode all apply.
 * @param paint  graphics state used to fill the canvas
 */
SKITY_C_API void skity_canvas_draw_paint(skity_canvas canvas,
                                         skity_paint paint);

/**
 * @brief Draw a single point at (@p x, @p y) using @p paint. The paint's
 *        stroke width determines the point size.
 * @param x      point position on the x-axis
 * @param y      point position on the y-axis
 * @param paint  stroke width, color, blend, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_point(skity_canvas canvas, float x, float y,
                                         skity_paint paint);

/**
 * @brief Draw a line segment from (@p x0, @p y0) to (@p x1, @p y1) using
 *        @p paint. The stroke width sets the line thickness; the paint's cap
 *        controls the end caps; the paint style is treated as stroke.
 * @param x0     start of the line segment on the x-axis
 * @param y0     start of the line segment on the y-axis
 * @param x1     end of the line segment on the x-axis
 * @param y1     end of the line segment on the y-axis
 * @param paint  stroke width, blend, color, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_line(skity_canvas canvas, float x0, float y0,
                                        float x1, float y1, skity_paint paint);

/**
 * @brief Draw the rectangle @p rect using @p paint.
 * @param rect   rectangle to draw
 * @param paint  stroke or fill, blend, color, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_rect(skity_canvas canvas,
                                        const skity_rect* rect,
                                        skity_paint paint);

/**
 * @brief Draw a circle centered at (@p cx, @p cy) with the given @p radius
 *        using @p paint.
 * @param cx      circle center on the x-axis
 * @param cy      circle center on the y-axis
 * @param radius  half the diameter of the circle
 * @param paint   stroke or fill, blend, color, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_circle(skity_canvas canvas, float cx,
                                          float cy, float radius,
                                          skity_paint paint);

/**
 * @brief Draw the oval bounded by @p oval using @p paint.
 * @param oval   rectangle bounds of the oval
 * @param paint  stroke or fill, blend, color, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_oval(skity_canvas canvas,
                                        const skity_rect* oval,
                                        skity_paint paint);

/**
 * @brief Draw the rounded rectangle bounded by @p rect with corner radii
 *        (@p rx, @p ry) using @p paint.
 * @param rect   bounds of the rounded rectangle to draw
 * @param rx     x-axis radius of the oval describing the rounded corners
 * @param ry     y-axis radius of the oval describing the rounded corners
 * @param paint  stroke or fill, blend, color, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_round_rect(skity_canvas canvas,
                                              const skity_rect* rect, float rx,
                                              float ry, skity_paint paint);

/**
 * @brief Draw the rounded rectangle bounded by @p rect with per-corner radii
 *        @p radii using @p paint. Unlike skity_canvas_draw_round_rect, each
 *        corner can have independent x-axis and y-axis radii.
 * @param rect   bounds of the rounded rectangle to draw
 * @param radii  corner radii as (x, y) pairs in the order top-left,
 *               top-right, bottom-right, bottom-left; four entries
 * @param paint  stroke or fill, blend, color, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_rrect(skity_canvas canvas,
                                         const skity_rect* rect,
                                         const skity_vec2* radii,
                                         skity_paint paint);

/**
 * @brief Draw @p path using @p paint.
 * @param path   path to draw
 * @param paint  stroke or fill, blend, color, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_path(skity_canvas canvas, skity_path path,
                                        skity_paint paint);

/**
 * @brief Draw @p image with its top-left corner at (@p x, @p y), using the
 *        default sampling and no paint.
 * @param image  image to draw
 * @param x      position of the image's top-left corner on the x-axis
 * @param y      position of the image's top-left corner on the y-axis
 */
SKITY_C_API void skity_canvas_draw_image(skity_canvas canvas, skity_image image,
                                         float x, float y);

/**
 * @brief Draw @p image with its top-left corner at (@p x, @p y), filtered
 *        with @p sampling and modulated by @p paint.
 * @param image     image to draw
 * @param x         position of the image's top-left corner on the x-axis
 * @param y         position of the image's top-left corner on the y-axis
 * @param sampling  sampling options used to filter the image; NULL to use the
 * default
 * @param paint     paint applied to modulate the image; NULL to draw without a
 * paint
 */
SKITY_C_API void skity_canvas_draw_image_with_sampling(
    skity_canvas canvas, skity_image image, float x, float y,
    const skity_sampling_options* sampling, skity_paint paint);

/**
 * @brief Draw @p image scaled to fill @p dst, filtered with @p sampling and
 *        modulated by @p paint.
 * @param image     image to draw
 * @param dst       destination rectangle to draw into
 * @param sampling  sampling options used to filter the image; NULL to use the
 * default
 * @param paint     paint applied to modulate the image; NULL to draw without a
 * paint
 */
SKITY_C_API void skity_canvas_draw_image_to_rect(
    skity_canvas canvas, skity_image image, const skity_rect* dst,
    const skity_sampling_options* sampling, skity_paint paint);

/**
 * @brief Draw the sub-rectangle @p src of @p image into the destination
 *        rectangle @p dst, filtered with @p sampling and modulated by @p paint.
 * @param image     image to draw
 * @param src       sub-rectangle of the image to draw
 * @param dst       destination rectangle to draw into
 * @param sampling  sampling options used to filter the image
 * @param paint     paint applied to modulate the image; NULL to draw without a
 * paint
 */
SKITY_C_API void skity_canvas_draw_image_rect(
    skity_canvas canvas, skity_image image, const skity_rect* src,
    const skity_rect* dst, const skity_sampling_options* sampling,
    skity_paint paint);

/**
 * @brief Draw @p blob with its origin at (@p x, @p y) using @p paint. The
 *        glyphs are taken from the blob; the paint supplies color, blend
 *        mode, and related state.
 * @param blob   text blob to draw
 * @param x      origin of the text blob on the x-axis
 * @param y      baseline of the text blob on the y-axis
 * @param paint  color, blend, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_text_blob(skity_canvas canvas,
                                             skity_text_blob blob, float x,
                                             float y, skity_paint paint);

/**
 * @brief Combine the rounded rectangle defined by @p rect and corner radii
 *        (@p rx, @p ry) with the current clip using @p op, replacing the
 *        clip with the result.
 * @param rect  bounds of the rounded rectangle to combine with the clip
 * @param rx    x-axis radius of the oval describing the rounded corners
 * @param ry    y-axis radius of the oval describing the rounded corners
 * @param op    clip operation to apply
 */
SKITY_C_API void skity_canvas_clip_rrect(skity_canvas canvas,
                                         const skity_rect* rect, float rx,
                                         float ry, skity_clip_op op);

/**
 * @brief Save the current matrix and clip, and allocate a backend render
 *        target for subsequent drawing. A matching skity_canvas_restore
 *        discards the layer and composites it back into the current context,
 *        applying @p paint's alpha, blend mode, and mask filter.
 * @param bounds  location and size of the layer's backing store
 * @param paint   alpha, blend mode, and mask filter applied when the layer
 *                is composited back to the context
 * @return the depth of the save stack after this save
 */
SKITY_C_API int32_t skity_canvas_save_layer(skity_canvas canvas,
                                            const skity_rect* bounds,
                                            skity_paint paint);

/**
 * @brief Draw an arc that is part of the oval bounded by @p oval, sweeping
 *        from @p start_angle to @p start_angle plus @p sweep_angle (both in
 *        degrees). A @p start_angle of zero starts at the right-middle edge of
 *        the oval; positive @p sweep_angle sweeps clockwise, negative sweeps
 *        counter-clockwise, and the sweep may exceed 360 degrees. When
 *        @p use_center is non-zero the arc is drawn as a wedge that includes
 *        lines from the oval's center to the arc end points; otherwise only
 *        the arc between the end points is drawn. Nothing is drawn if @p oval
 *        is empty or @p sweep_angle is zero.
 * @param oval         bounds of the oval containing the arc
 * @param start_angle  angle in degrees where the arc begins
 * @param sweep_angle  sweep angle in degrees; positive is clockwise
 * @param use_center   non-zero to include the oval's center, forming a wedge
 * @param paint        stroke or fill, blend, color, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_arc(skity_canvas canvas,
                                       const skity_rect* oval,
                                       float start_angle, float sweep_angle,
                                       uint32_t use_center, skity_paint paint);

/**
 * @brief Draw the ring shape formed by the rounded rectangle @p outer minus
 *        the rounded rectangle @p inner. Each rounded rectangle is described
 *        by its bounds and corner radii.
 * @param outer     bounds of the outer rounded rectangle
 * @param outer_rx  x-axis radius of the outer corners
 * @param outer_ry  y-axis radius of the outer corners
 * @param inner     bounds of the inner rounded rectangle (the hole)
 * @param inner_rx  x-axis radius of the inner corners
 * @param inner_ry  y-axis radius of the inner corners
 * @param paint     stroke or fill, blend, color, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_drrect(skity_canvas canvas,
                                          const skity_rect* outer,
                                          float outer_rx, float outer_ry,
                                          const skity_rect* inner,
                                          float inner_rx, float inner_ry,
                                          skity_paint paint);

/**
 * @brief Write the current total matrix (the accumulated transform applied
 *        to the canvas) to @p out.
 * @param out  receives the total matrix; must not be NULL
 */
SKITY_C_API void skity_canvas_get_total_matrix(skity_canvas canvas,
                                               skity_matrix* out);

/**
 * @brief Write the current clip bounds, expressed in local (pre-transform)
 *        coordinates, to @p out.
 * @param out  receives the local clip bounds; must not be NULL
 */
SKITY_C_API void skity_canvas_get_local_clip_bounds(skity_canvas canvas,
                                                    skity_rect* out);

/**
 * @brief Test whether @p rect is entirely outside the current clip.
 * @param rect  rectangle to test
 * @return 1 if @p rect is fully outside the clip and the draw can be skipped,
 *         0 otherwise
 */
SKITY_C_API uint32_t skity_canvas_quick_reject(skity_canvas canvas,
                                               const skity_rect* rect);

/**
 * @brief Draw a run of @p count glyphs at the given positions using @p font
 *        and @p paint. @p glyphs, @p positions_x, and @p positions_y must
 *        each contain at least @p count entries.
 * @param count        number of glyphs to draw
 * @param glyphs       array of glyph IDs, length @p count
 * @param positions_x  x position of each glyph, length @p count
 * @param positions_y  y position of each glyph, length @p count
 * @param font         font used to render the glyphs
 * @param paint        color, blend, and so on, used to draw
 */
SKITY_C_API void skity_canvas_draw_glyphs(skity_canvas canvas, uint32_t count,
                                          const uint16_t* glyphs,
                                          const float* positions_x,
                                          const float* positions_y,
                                          skity_font font, skity_paint paint);

/**
 * @brief Submit accumulated draw commands to the GPU backend. Must be called
 *        before skity_surface_flush. On OpenGL backends this may leave the
 *        stencil buffer dirty and change stencil/color state; on Vulkan
 *        backends it fills the current command buffer and changes the bound
 *        pipeline.
 */
SKITY_C_API void skity_canvas_flush(skity_canvas canvas);

/** @brief Return the canvas width in pixels. */
SKITY_C_API uint32_t skity_canvas_get_width(skity_canvas canvas);

/** @brief Return the canvas height in pixels. */
SKITY_C_API uint32_t skity_canvas_get_height(skity_canvas canvas);

/**
 * @brief Create a software (CPU-rasterized) canvas drawing into @p bitmap.
 *
 *        The returned canvas is owning: release it with skity_canvas_destroy,
 *        which also deletes the underlying Canvas. The bitmap must outlive
 *        the canvas.
 *
 * @param bitmap  bitmap that receives the drawing
 * @return a canvas handle, or NULL on failure
 */
SKITY_C_API skity_canvas skity_canvas_make_software_canvas(skity_bitmap bitmap);

/**
 * @brief Release the canvas wrapper. For a canvas obtained from a surface or
 *        recorder the underlying Canvas object is not deleted here; for a
 *        canvas created with skity_canvas_make_software_canvas the underlying
 *        object is deleted as well.
 */
SKITY_C_API void skity_canvas_destroy(skity_canvas canvas);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CANVAS_H
