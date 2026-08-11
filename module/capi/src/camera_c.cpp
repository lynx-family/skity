// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_camera.h>

#include <skity/geometry/camera.hpp>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/vector.hpp>

#include "handle.hpp"

namespace {

skity::Camera* camera_of(skity_camera handle) {
  auto* w =
      skity::capi::resolve<skity_camera_s>(handle, SKITY_OBJECT_TYPE_CAMERA);
  return w ? static_cast<skity::Camera*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

skity_camera skity_camera_create(float viewport_width, float viewport_height) {
  auto cam = std::make_shared<skity::Camera>(viewport_width, viewport_height);
  return skity::capi::alloc_handle<skity_camera_s>(
      SKITY_OBJECT_TYPE_CAMERA, SKITY_HANDLE_OWNING, std::move(cam));
}

void skity_camera_destroy(skity_camera camera) {
  skity::capi::destroy_handle<skity_camera_s>(camera, SKITY_OBJECT_TYPE_CAMERA);
}

void skity_camera_set_position(skity_camera camera,
                               const skity_vec4* position) {
  auto* c = camera_of(camera);
  if (c != nullptr && position != nullptr) {
    c->SetPosition(*reinterpret_cast<const skity::Point*>(position));
  }
}

void skity_camera_look_at(skity_camera camera, const skity_vec4* target) {
  auto* c = camera_of(camera);
  if (c != nullptr && target != nullptr) {
    c->LookAt(*reinterpret_cast<const skity::Point*>(target));
  }
}

void skity_camera_set_camera_dist(skity_camera camera, float dist) {
  if (auto* c = camera_of(camera)) c->SetCameraDist(dist);
}

void skity_camera_set_rotation(skity_camera camera,
                               const skity_matrix* rotation) {
  auto* c = camera_of(camera);
  if (c != nullptr && rotation != nullptr) {
    c->SetRotation(*reinterpret_cast<const skity::Matrix*>(rotation));
  }
}

void skity_camera_get_fixed_camera(skity_camera camera, skity_matrix* out) {
  auto* c = camera_of(camera);
  if (c == nullptr || out == nullptr) return;
  *reinterpret_cast<skity::Matrix*>(out) = c->GetFixedCamera();
}

void skity_camera_get_camera(skity_camera camera, skity_matrix* out) {
  auto* c = camera_of(camera);
  if (c == nullptr || out == nullptr) return;
  *reinterpret_cast<skity::Matrix*>(out) = c->GetCamera();
}

}  // extern "C"
