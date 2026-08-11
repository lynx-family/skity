// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CAMERA_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CAMERA_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 3D camera used to build a view / projection matrix for fake-3D
 *        rendering, mirroring skity::Camera.
 */
SKITY_C_DEFINE_HANDLE(skity_camera);

/**
 * @brief Create a 3D camera for a viewport of @p viewport_width x
 *        @p viewport_height.
 * @param viewport_width   viewport width in pixels
 * @param viewport_height  viewport height in pixels
 * @return                 a new camera handle, or NULL on failure
 */
SKITY_C_API skity_camera skity_camera_create(float viewport_width,
                                             float viewport_height);

/** @brief Release the camera handle. Safe on NULL. */
SKITY_C_API void skity_camera_destroy(skity_camera camera);

/**
 * @brief Set the camera position in 3D space.
 * @param position  camera position interpreted as a Point / Vec4
 */
SKITY_C_API void skity_camera_set_position(skity_camera camera,
                                           const skity_vec4* position);

/**
 * @brief Point the camera at @p target.
 * @param target  look-at target interpreted as a Point / Vec4
 */
SKITY_C_API void skity_camera_look_at(skity_camera camera,
                                      const skity_vec4* target);

/**
 * @brief Set the camera distance used when computing the perspective / view
 *        matrix.
 * @param dist  distance from the eye to the near plane
 */
SKITY_C_API void skity_camera_set_camera_dist(skity_camera camera, float dist);

/**
 * @brief Override the camera rotation.
 * @param rotation  4x4 column-major rotation matrix
 */
SKITY_C_API void skity_camera_set_rotation(skity_camera camera,
                                           const skity_matrix* rotation);

/**
 * @brief Write the fixed view-projection matrix into @p out.
 * @param out  points to a skity_matrix; receives 16 column-major floats
 */
SKITY_C_API void skity_camera_get_fixed_camera(skity_camera camera,
                                               skity_matrix* out);

/**
 * @brief Write the current view-projection matrix (reflecting position / look
 *        target / rotation / distance) into @p out.
 * @param out  points to a skity_matrix; receives 16 column-major floats
 */
SKITY_C_API void skity_camera_get_camera(skity_camera camera,
                                         skity_matrix* out);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CAMERA_H
