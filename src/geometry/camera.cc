// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <skity/geometry/camera.hpp>
#include <skity/geometry/point.hpp>
#include <skity/geometry/quaternion.hpp>

#include "src/geometry/glm_helper.hpp"

namespace skity {

constexpr float DEFALUT_CAMERA_DIST = 879.13f;

namespace {

Matrix CameraMatrix(float viewport_width, float viewport_height,
                    const Point& camera_pos, const Point& lookat_target,
                    float camera_dist, const Matrix& rotate) {
  auto view = glm::lookAt(glm::vec3{camera_pos.x, camera_pos.y, -camera_pos.z},
                          {lookat_target.x, lookat_target.y, lookat_target.z},
                          {0.f, 1.f, 0.f});

  auto view_angle = std::atan(viewport_height * 0.5f / camera_dist);
  auto projection = glm::perspective(
      view_angle * 2, viewport_width / viewport_height, 0.f, camera_dist);

  Matrix neg_z;
  neg_z.Set(2, 2, -1);
  Matrix total = Matrix::Translate(viewport_width / 2, viewport_height / 2) *
                 Matrix::Scale(viewport_width / 2, viewport_height / 2) *
                 FromGLM(projection) * rotate * FromGLM(view) * neg_z;
  return total;
}
}  // namespace

Camera::Camera(float viewport_width, float viewport_height)
    : viewport_width_(viewport_width),
      viewport_height_(viewport_height),
      camera_dist_(DEFALUT_CAMERA_DIST) {}

void Camera::SetPosition(const Point& pos) { position_ = pos; }

void Camera::LookAt(const Point& target) { lookat_target_ = target; }

void Camera::SetCameraDist(float camera_dist) { camera_dist_ = camera_dist; }

void Camera::SetRotation(const Matrix& rotate) { rotate_ = rotate; }

Matrix Camera::GetFixedCamera() {
  auto viewport_center_x = viewport_width_ / 2;
  auto viewport_center_y = viewport_height_ / 2;

  Point fixed_camera_pos{viewport_center_x, viewport_center_y, -camera_dist_,
                         1};
  Point fixed_look_target{viewport_center_x, viewport_center_y,
                          -camera_dist_ - 1, 1};

  return CameraMatrix(viewport_width_, viewport_height_, fixed_camera_pos,
                      fixed_look_target, camera_dist_, Matrix{});
}

Matrix Camera::GetCamera() {
  return CameraMatrix(viewport_width_, viewport_height_, position_,
                      lookat_target_, camera_dist_, rotate_);
}

}  // namespace skity
