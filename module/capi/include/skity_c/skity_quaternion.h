// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_QUATERNION_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_QUATERNION_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build a rotation matrix from XYZ exterior Euler angles, mirroring
 *        skity::Quaternion::EulerToMatrix.
 * @param alpha   rotation angle around the X axis (radians)
 * @param beta    rotation angle around the Y axis (radians)
 * @param gamma   rotation angle around the Z axis (radians)
 * @param out     points to a skity_matrix; receives 16 column-major floats.
 *                Each input angle should be less than 2*PI for a stable result.
 */
SKITY_C_API void skity_quaternion_euler_to_matrix(float alpha, float beta,
                                                  float gamma,
                                                  skity_matrix* out);

/**
 * @brief Build a rotation matrix from a normalized axis and angle, mirroring
 *        skity::Quaternion (FromAxisAngle + ToMatrix).
 * @param axis   normalized rotation axis (3 floats, vec3)
 * @param angle  rotation angle (radians)
 * @param out    points to a skity_matrix; receives 16 column-major floats
 */
SKITY_C_API void skity_quaternion_axis_angle_to_matrix(const skity_vec3* axis,
                                                       float angle,
                                                       skity_matrix* out);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_QUATERNION_H
