// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_QUATERNION_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_QUATERNION_HPP

// Quaternion rotation helpers of the header-only RAII layer: rotation-matrix
// builders mirroring the legacy skity::Quaternion statics. Free functions,
// like the StrokePath / Op helpers.

#include <skity_c/skity_quaternion.h>

#include <skity_hpp/skity_types.hpp>

namespace skity {
namespace raii {

/**
 * Rotation matrix from XYZ exterior Euler angles (radians). Angles should be
 * below 2π for a stable result (legacy Quaternion::EulerToMatrix).
 */
inline Matrix QuaternionEulerToMatrix(float alpha, float beta, float gamma) {
  Matrix out;
  skity_quaternion_euler_to_matrix(alpha, beta, gamma, &out);
  return out;
}

/**
 * Rotation matrix from a normalized axis (Vec3) and an angle in radians
 * (legacy Quaternion::FromAxisAngle + ToMatrix).
 */
inline Matrix QuaternionAxisAngleToMatrix(const Vec3& axis, float angle) {
  Matrix out;
  skity_quaternion_axis_angle_to_matrix(&axis, angle, &out);
  return out;
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_QUATERNION_HPP
