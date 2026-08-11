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

// ----- picture recorder: records draw commands into a display list -----

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
 * @brief Write the display list's cull bounds into @p out_bounds.
 * @param list        the display list
 * @param out_bounds  receives the bounds rectangle
 */
SKITY_C_API void skity_display_list_get_bounds(skity_display_list list,
                                               skity_rect* out_bounds);

/** @brief Return the number of recorded draw ops in the list. */
SKITY_C_API uint32_t skity_display_list_get_op_count(skity_display_list list);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_RECORDER_H
