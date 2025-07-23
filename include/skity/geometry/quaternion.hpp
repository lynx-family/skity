// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GEOMETRY_QUATERNION_HPP
#define INCLUDE_SKITY_GEOMETRY_QUATERNION_HPP

#include <skity/geometry/matrix.hpp>
#include <skity/geometry/point.hpp>
#include <utility>

namespace skity {

class SKITY_API Quaternion final {
 public:
  /**
   * XYZ exterior rotation order.
   */
  static Matrix EulerToMatrix(float alpha, float beta, float gamma);

  /**
   * XYZ exterior rotation order.
   * Input radian of three angles must be less than 2 * PI. Furthermore, we'd
   * better use the angles that is not nearly 2 * PI.
   */
  static Quaternion FromEuler(float alpha, float beta, float gamma);

  /**
   * Input radian of three angles must be less than 2 * PI. Furthermore, we'd
   * better use the angles that is not nearly 2 * PI.
   */
  static Quaternion FromAxisAngle(Vec3 axis, float angle);

  static Quaternion FromXYZW(float x, float y, float z, float w);

  Quaternion operator*(const Quaternion& right) const;

  Quaternion Multiple(const Quaternion& right) const;

  Quaternion Slerp(const Quaternion& end, float t) const;

  Matrix ToMatrix() const;

  std::pair<Vec3, float> ToAxisAngle() const;

  Quaternion Reciprocal() const;

  Quaternion Negative() const;

  float IncludeAngle(const Quaternion& right) const;

  float X() const { return x_; }
  float Y() const { return y_; }
  float Z() const { return z_; }
  float W() const { return w_; }

 private:
  Quaternion(float x, float y, float z, float w);

  float Dot(const Quaternion& right) const;

  const float x_;
  const float y_;
  const float z_;
  const float w_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GEOMETRY_QUATERNION_HPP
