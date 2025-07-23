// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GEOMETRY_CAMERA_HPP
#define INCLUDE_SKITY_GEOMETRY_CAMERA_HPP

#include <skity/geometry/matrix.hpp>
#include <skity/geometry/point.hpp>

namespace skity {

// Camera is for vp matrix in MVP
class SKITY_API Camera final {
 public:
  Camera(float viewport_width, float viewport_height);

  void SetPosition(const Point& pos);
  void LookAt(const Point& target);
  void SetCameraDist(float camera_dist);
  void SetRotation(const Matrix& rotate);

  Matrix GetFixedCamera();
  Matrix GetCamera();

 private:
  float viewport_width_;
  float viewport_height_;
  Point position_;
  Point lookat_target_;
  float camera_dist_;
  Matrix rotate_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GEOMETRY_CAMERA_HPP
