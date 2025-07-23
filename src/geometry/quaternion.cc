// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <skity/geometry/point.hpp>
#include <skity/geometry/quaternion.hpp>

#include "src/geometry/math.hpp"

namespace skity {

Matrix Quaternion::EulerToMatrix(float alpha, float beta, float gamma) {
  const auto cos_alpha = std::cos(alpha);
  const auto sin_alpha = std::sin(alpha);
  const auto cos_beta = std::cos(beta);
  const auto sin_beta = std::sin(beta);
  const auto cos_gamma = std::cos(gamma);
  const auto sin_gamma = std::sin(gamma);

  return Matrix(cos_beta * cos_gamma, cos_beta * sin_gamma, -sin_beta, 0,
                cos_gamma * sin_alpha * sin_beta - cos_alpha * sin_gamma,
                cos_alpha * cos_gamma + sin_alpha * sin_beta * sin_gamma,
                cos_beta * sin_alpha, 0,
                sin_alpha * sin_gamma + cos_alpha * cos_gamma * sin_beta,
                cos_alpha * sin_beta * sin_gamma - cos_gamma * sin_alpha,
                cos_alpha * cos_beta, 0, 0, 0, 0, 1);
}

Quaternion Quaternion::FromEuler(float alpha, float beta, float gamma) {
  const auto cos_alpha = std::cos(alpha / 2);
  const auto sin_alpha = std::sin(alpha / 2);
  const auto cos_beta = std::cos(beta / 2);
  const auto sin_beta = std::sin(beta / 2);
  const auto cos_gamma = std::cos(gamma / 2);
  const auto sin_gamma = std::sin(gamma / 2);
  return Quaternion(
      sin_alpha * cos_beta * cos_gamma - cos_alpha * sin_beta * sin_gamma,
      cos_alpha * sin_beta * cos_gamma + sin_alpha * cos_beta * sin_gamma,
      cos_alpha * cos_beta * sin_gamma - sin_alpha * sin_beta * cos_gamma,
      cos_alpha * cos_beta * cos_gamma + sin_alpha * sin_beta * sin_gamma);
}

Quaternion Quaternion::FromAxisAngle(Vec3 axis, float angle) {
  axis = glm::normalize(axis);
  const auto cos_angle = std::cos(angle / 2);
  const auto sin_angle = std::sin(angle / 2);
  return Quaternion(sin_angle * axis.x, sin_angle * axis.y, sin_angle * axis.z,
                    cos_angle);
}

Quaternion Quaternion::FromXYZW(float x, float y, float z, float w) {
  Vec4 xyzw{
      x,
      y,
      z,
      w,
  };
  const auto length = glm::length(xyzw);
  return Quaternion(xyzw.x / length, xyzw.y / length, xyzw.z / length,
                    xyzw.w / length);
}

Quaternion::Quaternion(float x, float y, float z, float w)
    : x_(x), y_(y), z_(z), w_(w) {}

float Quaternion::Dot(const Quaternion& right) const {
  return x_ * right.x_ + y_ * right.y_ + z_ * right.z_ + w_ * right.w_;
}

Quaternion Quaternion::operator*(const Quaternion& right) const {
  return Multiple(right);
}

Quaternion Quaternion::Multiple(const Quaternion& right) const {
  return Quaternion(
      w_ * right.x_ + x_ * right.w_ + y_ * right.z_ - z_ * right.y_,
      w_ * right.y_ + y_ * right.w_ + z_ * right.x_ - x_ * right.z_,
      w_ * right.z_ + z_ * right.w_ + x_ * right.y_ - y_ * right.x_,
      w_ * right.w_ - x_ * right.x_ - y_ * right.y_ - z_ * right.z_);
}

Quaternion Quaternion::Slerp(const Quaternion& end, float t) const {
  const auto cos_theta = Dot(end);
  if (std::fabs(cos_theta) < 0.999) {
    // Quaternion slerp
    const auto theta = std::acos(cos_theta);
    const auto sin_theta = std::sin(theta);
    const auto complement_tt = std::sin((1.f - t) * theta) / sin_theta;
    auto tt = sin(t * theta) / sin_theta;
    return Quaternion(
        x_ * complement_tt + end.x_ * tt, y_ * complement_tt + end.y_ * tt,
        z_ * complement_tt + end.z_ * tt, w_ * complement_tt + end.w_ * tt);
  } else {
    // Quaternion lerp
    const auto complement_t = 1.f - t;
    Vec4 xyzw{
        x_ * complement_t + end.x_ * t,
        y_ * complement_t + end.y_ * t,
        z_ * complement_t + end.z_ * t,
        w_ * complement_t + end.w_ * t,
    };
    const auto length = glm::length(xyzw);
    return Quaternion(xyzw.x / length, xyzw.y / length, xyzw.z / length,
                      xyzw.w / length);
  }
}

Matrix Quaternion::ToMatrix() const {
  const auto xx = x_ * x_;

  const auto xy = x_ * y_;
  const auto xz = x_ * z_;
  const auto xw = x_ * w_;

  const auto yy = y_ * y_;
  const auto yz = y_ * z_;
  const auto yw = y_ * w_;

  const auto zz = z_ * z_;
  const auto zw = z_ * w_;

  // clang-format off
  return Matrix(1 - 2 * (yy + zz), 2 * (xy + zw),     2 * (xz - yw),     0,
                2 * (xy - zw),     1 - 2 * (xx + zz), 2 * (yz + xw),     0,
                2 * (xz + yw),     2 * (yz - xw),     1 - 2 * (xx + yy), 0,
                0,                 0,                 0,                 1);
  // clang-format on
}

std::pair<Vec3, float> Quaternion::ToAxisAngle() const {
  const auto angle = std::acos(w_);
  const auto sin_angle = std::sin(angle);
  if (!FloatNearlyZero(sin_angle)) {
    return {{x_ / sin_angle, y_ / sin_angle, z_ / sin_angle}, 2 * angle};
  }
  return {{}, 0.f};
}

Quaternion Quaternion::Reciprocal() const {
  return Quaternion{-x_, -y_, -z_, w_};
}

Quaternion Quaternion::Negative() const {
  return Quaternion{-x_, -y_, -z_, -w_};
}

float Quaternion::IncludeAngle(const Quaternion& right) const {
  float cos_angle = Dot(right);
  return std::acos(cos_angle);
}

}  // namespace skity
