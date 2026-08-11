// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_quaternion.h>

#include <skity/geometry/matrix.hpp>
#include <skity/geometry/quaternion.hpp>
#include <skity/geometry/vector.hpp>

#include "handle.hpp"

extern "C" {

void skity_quaternion_euler_to_matrix(float alpha, float beta, float gamma,
                                      skity_matrix* out) {
  if (out == nullptr) return;
  skity::Matrix m = skity::Quaternion::EulerToMatrix(alpha, beta, gamma);
  *reinterpret_cast<skity::Matrix*>(out) = m;
}

void skity_quaternion_axis_angle_to_matrix(const skity_vec3* axis, float angle,
                                           skity_matrix* out) {
  if (out == nullptr || axis == nullptr) return;
  skity::Vec3 v(axis->e[0], axis->e[1], axis->e[2]);
  skity::Matrix m = skity::Quaternion::FromAxisAngle(v, angle).ToMatrix();
  *reinterpret_cast<skity::Matrix*>(out) = m;
}

}  // extern "C"
