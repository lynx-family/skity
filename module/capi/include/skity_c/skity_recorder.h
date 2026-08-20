// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_RECORDER_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_RECORDER_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_canvas.h>
#include <skity_c/skity_types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Records canvas draw commands into a replayable display list,
 *        mirroring skity::PictureRecorder.
 */
SKITY_C_DEFINE_HANDLE(skity_picture_recorder);

/**
 * @brief A recorded, replayable set of draw commands, mirroring
 *        skity::DisplayList. Draw it onto any canvas with
 *        skity_display_list_draw.
 */
SKITY_C_DEFINE_HANDLE(skity_display_list);

// ----- picture recorder: records draw commands into a display list ------

/** @brief Create a new picture recorder. */
SKITY_C_API skity_picture_recorder skity_picture_recorder_create(void);

/** @brief Release the picture recorder handle. Safe on NULL. */
SKITY_C_API void skity_picture_recorder_destroy(
    skity_picture_recorder recorder);

/**
 * @brief Begin recording. Subsequent draws on the recorder's canvas are
 *        captured into a new display list.
 * @param recorder  the recorder handle
 * @param bounds    cull rect of the recorded content, or NULL for an unbounded
 *                  recording
 */
SKITY_C_API void skity_picture_recorder_begin(skity_picture_recorder recorder,
                                              const skity_rect* bounds);

/**
 * @brief Options controlling how the recorded display list is built. Mirrors
 *        skity::DisplayListBuildOptions.
 */
typedef struct skity_display_list_build_options {
  /**
   * Non-zero to build an RTree of recorded op bounds. Required for
   * skity_display_list_search and the cull-rect variant of
   * skity_display_list_draw to do partial work; without it both fall back to
   * whole-list behavior.
   */
  uint32_t build_rtree;
} skity_display_list_build_options;

/**
 * @brief Begin recording with explicit build options.
 *
 * Same as skity_picture_recorder_begin, plus control over the display list
 * build. Passing NULL @p options is equivalent to
 * skity_picture_recorder_begin.
 *
 * @param recorder  the recorder handle
 * @param bounds    cull rect of the recorded content, or NULL for an unbounded
 *                  recording
 * @param options   build options, or NULL for the defaults
 */
SKITY_C_API void skity_picture_recorder_begin_with_options(
    skity_picture_recorder recorder, const skity_rect* bounds,
    const skity_display_list_build_options* options);

/**
 * @brief Return the storage offset of the most recently recorded op, for use
 *        with skity_display_list_get_op_paint_by_offset.
 *
 * Mirrors RecordingCanvas::GetLastOpOffset. Call it right after the draw call
 * of interest, while still recording.
 *
 * @param recorder  the recorder handle
 * @return          the op's byte offset into the finished display list, or -1
 *                  when nothing has been recorded yet (or recording has not
 *                  begun)
 */
SKITY_C_API int32_t
skity_picture_recorder_get_last_op_offset(skity_picture_recorder recorder);

/**
 * @brief Return the recording canvas. It is owned by the recorder (a
 *        non-owning handle) and stays valid until
 *        skity_picture_recorder_finish or skity_picture_recorder_destroy.
 *        Draw on it with the regular skity_canvas_* API to record commands.
 * @return the recording canvas handle, or NULL if recording has not begun
 */
SKITY_C_API skity_canvas
skity_picture_recorder_get_canvas(skity_picture_recorder recorder);

/**
 * @brief Finish recording and produce a display list. The recording canvas
 *        handle is invalidated after this call.
 * @param recorder  the recorder handle
 * @param out       receives the new display list handle
 * @return          SKITY_SUCCESS, or SKITY_ERROR_INVALID_ARGUMENT if the
 *                  recorder has not begun recording
 */
SKITY_C_API skity_result skity_picture_recorder_finish(
    skity_picture_recorder recorder, skity_display_list* out);

// ----- display list: a recorded, replayable set of draw commands -----

/** @brief Release the display list handle. Safe on NULL. */
SKITY_C_API void skity_display_list_destroy(skity_display_list list);

/**
 * @brief Playback the recorded commands onto @p canvas.
 * @param list    the display list to replay
 * @param canvas  destination canvas receiving the draws
 */
SKITY_C_API void skity_display_list_draw(skity_display_list list,
                                         skity_canvas canvas);

/**
 * @brief Playback only the recorded commands that intersect @p cull_rect
 *        (partial redraw), keeping the save / clip / matrix state those
 *        commands depend on.
 *
 * Requires a display list recorded with build_rtree set; otherwise this falls
 * back to replaying the whole list. An empty or NULL @p cull_rect draws
 * nothing. Mirrors DisplayList::Draw(canvas, cull_rect).
 *
 * @param list       the display list to replay
 * @param canvas     destination canvas receiving the draws
 * @param cull_rect  damage region to redraw, or NULL to draw nothing
 */
SKITY_C_API void skity_display_list_draw_with_cull_rect(
    skity_display_list list, skity_canvas canvas, const skity_rect* cull_rect);

/**
 * @brief Return the offsets of recorded ops whose (device-space) bounds
 *        intersect @p rect, in recording order.
 *
 * Requires a display list recorded with build_rtree set; without it, 0 is
 * always returned. Offsets are opaque values only meaningful to this display
 * list — feed them to skity_display_list_get_op_paint_by_offset.
 *
 * Two-pass idiom: call with @p out_offsets NULL (or a small capacity) to learn
 * the count, then call again with a sufficiently large buffer. When @p capacity
 * is smaller than the result count, only @p capacity entries are written.
 *
 * @param list         the display list recorded with build_rtree
 * @param rect         query rectangle; an empty rect matches nothing
 * @param out_offsets  receives up to @p capacity op offsets, or NULL to only
 *                     query the count
 * @param capacity     number of entries @p out_offsets can hold
 * @return             the total number of matching ops
 */
SKITY_C_API uint32_t skity_display_list_search(skity_display_list list,
                                               const skity_rect* rect,
                                               int32_t* out_offsets,
                                               uint32_t capacity);

/**
 * @brief Return the damage region covering the ops intersecting @p rect,
 *        merged into mutually non-overlapping rectangles.
 *
 * Useful to compute the area that skity_display_list_draw_with_cull_rect will
 * touch (e.g. to union with a surface damage region). Requires build_rtree;
 * without it, 0 is always returned. Same two-pass idiom as
 * skity_display_list_search.
 *
 * @param list       the display list recorded with build_rtree
 * @param rect       query rectangle; an empty rect matches nothing
 * @param out_rects  receives up to @p capacity rectangles, or NULL to only
 *                   query the count
 * @param capacity   number of entries @p out_rects can hold
 * @return           the total number of result rectangles
 */
SKITY_C_API uint32_t skity_display_list_search_non_overlapping_drawn_rects(
    skity_display_list list, const skity_rect* rect, skity_rect* out_rects,
    uint32_t capacity);

/**
 * @brief Write the display list's cull bounds into @p out_bounds.
 * @param list        the display list
 * @param out_bounds  receives the bounds rectangle
 */
SKITY_C_API void skity_display_list_get_bounds(skity_display_list list,
                                               skity_rect* out_bounds);

/** @brief Return the number of recorded draw ops in the list. */
SKITY_C_API uint32_t skity_display_list_get_op_count(skity_display_list list);

/** @brief Recorded-content property bits, aligned with
 *         skity::DisplayList::Property. */
typedef enum {
  SKITY_DISPLAY_LIST_PROPERTY_NONE = 0,
  SKITY_DISPLAY_LIST_PROPERTY_SAVE_LAYER = 1 << 0, /**< contains a save_layer */
  SKITY_DISPLAY_LIST_PROPERTY_SHADER = 1 << 1, /**< some paint has a shader */
  SKITY_DISPLAY_LIST_PROPERTY_COLOR_FILTER = 1 << 2, /**< a paint has a color
                                                        filter */
  SKITY_DISPLAY_LIST_PROPERTY_MASK_FILTER = 1 << 3,  /**< a paint has a mask
                                                         filter */
  SKITY_DISPLAY_LIST_PROPERTY_IMAGE_FILTER = 1 << 4, /**< a paint has an image
                                                        filter */
} skity_display_list_property;

/**
 * @brief Return the display list's property bitmask, a combination of
 *        skity_display_list_property values. Mirrors the DisplayList::Has*()
 *        getters.
 * @param list  the display list
 * @return     property bits; 0 for an empty list
 */
SKITY_C_API uint32_t skity_display_list_get_properties(skity_display_list list);

/**
 * @brief Return the paint of the op recorded at @p offset, for in-place
 *        modification before replay.
 *
 * The returned handle is non-owning: the paint lives inside the display list's
 * storage, so the handle stays valid until @p list is destroyed, and
 * skity_paint_destroy on it only reclaims the wrapper. Mutations are seen by
 * the next skity_display_list_draw / draw_with_cull_rect.
 *
 * @param list    the display list the offset came from
 * @param offset  op offset from skity_display_list_search or
 *                skity_picture_recorder_get_last_op_offset
 * @return        a non-owning paint handle, or NULL when @p offset is invalid
 *                for this list or the op carries no paint (e.g. clip / matrix
 *                ops)
 */
SKITY_C_API skity_paint skity_display_list_get_op_paint_by_offset(
    skity_display_list list, int32_t offset);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_RECORDER_H
