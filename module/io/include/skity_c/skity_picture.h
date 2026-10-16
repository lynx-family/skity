// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PICTURE_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PICTURE_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_bitmap.h>
#include <skity_c/skity_canvas.h>
#include <skity_c/skity_context.h>
#include <skity_c/skity_image.h>
#include <skity_c/skity_recorder.h>
#include <skity_c/skity_stream.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

SKITY_C_DEFINE_HANDLE(skity_picture);

/**
 * @brief Serialization hooks, mirroring skity::SerialProc. All fields are
 *        optional: gpu_context serializes GPU-backed images through a live
 *        context, image_proc supplies custom pixel data for an image
 *        (return NULL to skip that image), userdata is passed back to
 *        image_proc.
 */
typedef struct skity_serial_proc {
  skity_context gpu_context; /**< context for GPU image reads (may be NULL) */
  skity_pixmap (*image_proc)(skity_image image, void* userdata);
  void* userdata;
} skity_serial_proc;

/**
 * @brief Capture a recorded display list for serialization
 *        (skity::Picture::MakeFromDisplayList).
 * @param dl  finished display list from skity_picture_recorder_finish
 * @return new picture handle, or NULL on failure
 */
SKITY_C_API skity_picture
skity_picture_create_from_display_list(skity_display_list dl);

/**
 * @brief Deserialize a picture from a stream
 *        (skity::Picture::MakeFromStream).
 * @param stream  read-stream positioned at the picture payload
 * @return new picture handle, or NULL when deserialization fails
 */
SKITY_C_API skity_picture
skity_picture_create_from_stream(skity_read_stream stream);

/** @brief Release the picture handle. Safe on NULL. */
SKITY_C_API void skity_picture_destroy(skity_picture picture);

/** @brief Cull rectangle the picture was recorded with. */
SKITY_C_API skity_rect skity_picture_get_cull_rect(skity_picture picture);

/**
 * @brief Replay the recorded drawing commands onto @p canvas
 *        (skity::Picture::PlayBack).
 */
SKITY_C_API void skity_picture_play_back(skity_picture picture,
                                         skity_canvas canvas);

/**
 * @brief Serialize the picture to @p stream (skity::Picture::Serialize).
 * @param stream  write-stream receiving the payload
 * @param proc    optional serialization hooks (may be NULL)
 */
SKITY_C_API void skity_picture_serialize(skity_picture picture,
                                         skity_write_stream stream,
                                         const skity_serial_proc* proc);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PICTURE_H
