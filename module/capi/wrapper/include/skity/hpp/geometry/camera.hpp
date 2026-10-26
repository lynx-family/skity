// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_HPP_GEOMETRY_CAMERA_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_HPP_GEOMETRY_CAMERA_HPP

#include <skity/c/skity_camera.h>

#include <skity/hpp/base.hpp>
#include <skity/hpp/types.hpp>

namespace skity {

/**
 * @class Camera
 *
 * Simulated 3D camera (legacy skity::Camera): position, look-at target,
 * distance and rotation, projected through a viewport.
 *
 */
class Camera : public detail::OwnHandle<skity_camera, skity_camera_destroy> {
 public:
  /** viewport_width / viewport_height size the projection. */
  Camera(float viewport_width, float viewport_height)
      : OwnHandle(skity_camera_create(viewport_width, viewport_height)) {}

  /** Eye position in world space (homogeneous point). */
  void SetPosition(const Vec4& position) {
    skity_camera_set_position(get(), &position);
  }

  /** Point the camera at target (homogeneous point). */
  void LookAt(const Vec4& target) { skity_camera_look_at(get(), &target); }

  /** Distance from the eye to the look-at target. */
  void SetCameraDist(float dist) { skity_camera_set_camera_dist(get(), dist); }

  /** Camera-space rotation applied before the view transform. */
  void SetRotation(const Matrix& rotation) {
    skity_camera_set_rotation(get(), &rotation);
  }

  /** The fixed (un-animated) view matrix — the classic Skia camera origin. */
  Matrix GetFixedCamera() const {
    Matrix out;
    skity_camera_get_fixed_camera(get(), &out);
    return out;
  }

  /** The current view matrix; concat onto a canvas to draw in camera space. */
  Matrix GetCamera() const {
    Matrix out;
    skity_camera_get_camera(get(), &out);
    return out;
  }
};

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_HPP_GEOMETRY_CAMERA_HPP
