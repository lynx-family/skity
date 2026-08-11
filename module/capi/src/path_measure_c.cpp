// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_path_measure.h>

#include <skity/geometry/vector.hpp>
#include <skity/graphic/path.hpp>
#include <skity/graphic/path_measure.hpp>

#include "handle.hpp"

namespace {

skity::PathMeasure* measure_of(skity_path_measure handle) {
  auto* w = skity::capi::resolve<skity_path_measure_s>(
      handle, SKITY_OBJECT_TYPE_PATH_MEASURE);
  return w ? static_cast<skity::PathMeasure*>(w->impl.get()) : nullptr;
}

std::shared_ptr<skity::Path> path_shared(skity_path handle) {
  return skity::capi::get_impl<skity_path_s, skity::Path>(
      handle, SKITY_OBJECT_TYPE_PATH);
}

}  // namespace

extern "C" {

skity_path_measure skity_path_measure_create(skity_path path,
                                             uint32_t force_closed,
                                             float res_scale) {
  auto p = path_shared(path);
  std::shared_ptr<void> impl;
  if (p != nullptr) {
    impl =
        std::make_shared<skity::PathMeasure>(*p, force_closed != 0, res_scale);
  } else {
    impl = std::make_shared<skity::PathMeasure>();
  }
  return skity::capi::alloc_handle<skity_path_measure_s>(
      SKITY_OBJECT_TYPE_PATH_MEASURE, SKITY_HANDLE_OWNING, std::move(impl));
}

void skity_path_measure_destroy(skity_path_measure measure) {
  skity::capi::destroy_handle<skity_path_measure_s>(
      measure, SKITY_OBJECT_TYPE_PATH_MEASURE);
}

uint32_t skity_path_measure_set_path(skity_path_measure measure,
                                     skity_path path, uint32_t force_closed) {
  auto* m = measure_of(measure);
  if (m == nullptr) {
    return 0u;
  }
  auto p = path_shared(path);
  m->SetPath(p.get(), force_closed != 0);
  return 1u;
}

float skity_path_measure_get_length(skity_path_measure measure) {
  auto* m = measure_of(measure);
  return m ? m->GetLength() : 0.f;
}

uint32_t skity_path_measure_get_pos_tan(skity_path_measure measure,
                                        float distance,
                                        skity_vec4* out_position,
                                        skity_vec4* out_tangent) {
  auto* m = measure_of(measure);
  if (m == nullptr) {
    return 0u;
  }
  skity::Point pos;
  skity::Vector tan;
  if (!m->GetPosTan(distance, &pos, &tan)) {
    return 0u;
  }
  if (out_position != nullptr) {
    *reinterpret_cast<skity::Vec4*>(out_position) = pos;
  }
  if (out_tangent != nullptr) {
    *reinterpret_cast<skity::Vec4*>(out_tangent) = tan;
  }
  return 1u;
}

uint32_t skity_path_measure_get_segment(skity_path_measure measure,
                                        float start_d, float stop_d,
                                        skity_path dst,
                                        uint32_t start_with_move_to) {
  auto* m = measure_of(measure);
  auto d = path_shared(dst);
  if (m == nullptr || d == nullptr) {
    return 0u;
  }
  return m->GetSegment(start_d, stop_d, d.get(), start_with_move_to != 0) ? 1u
                                                                          : 0u;
}

uint32_t skity_path_measure_is_closed(skity_path_measure measure) {
  auto* m = measure_of(measure);
  return (m != nullptr && m->IsClosed()) ? 1u : 0u;
}

uint32_t skity_path_measure_next_contour(skity_path_measure measure) {
  auto* m = measure_of(measure);
  return (m != nullptr && m->NextContour()) ? 1u : 0u;
}

}  // extern "C"
