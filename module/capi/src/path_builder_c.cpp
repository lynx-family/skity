// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/c/skity_path_builder.h>

#include <memory>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rrect.hpp>
#include <skity/graphic/path_builder.hpp>
#include <utility>

#include "handle.hpp"

namespace {

skity::PathBuilder* builder_of(skity_path_builder handle) {
  auto* w = skity::capi::resolve<skity_path_builder_s>(
      handle, SKITY_OBJECT_TYPE_PATH_BUILDER);
  return w ? w->impl.get() : nullptr;
}

skity::Path* path_of(skity_path handle, bool require_owning = false) {
  auto* w = skity::capi::resolve<skity_path_s>(handle, SKITY_OBJECT_TYPE_PATH);
  if (!w || (require_owning && !(w->header.flags & SKITY_HANDLE_OWNING))) {
    return nullptr;
  }
  return w->impl.get();
}

bool valid_dir(skity_path_direction dir) {
  return dir == SKITY_PATH_DIRECTION_CW || dir == SKITY_PATH_DIRECTION_CCW;
}

skity::Path::Direction to_dir(skity_path_direction dir) {
  return static_cast<skity::Path::Direction>(dir);
}

bool valid_mode(skity_path_add_mode mode) {
  return mode == SKITY_PATH_ADD_MODE_APPEND ||
         mode == SKITY_PATH_ADD_MODE_EXTEND;
}

skity::Rect to_rect(const skity_rect& rect) {
  return {rect.left, rect.top, rect.right, rect.bottom};
}

skity::RRect to_rrect(const skity_rect& rect, const skity_vec2 radii[4]) {
  skity::Vec2 values[4];
  for (size_t i = 0; i < 4; ++i) values[i] = {radii[i].x, radii[i].y};
  skity::RRect result;
  result.SetRectRadii(to_rect(rect), values);
  return result;
}

}  // namespace

extern "C" {

skity_path_builder skity_path_builder_create(void) {
  return skity::capi::alloc_handle<skity_path_builder_s>(
      SKITY_OBJECT_TYPE_PATH_BUILDER, SKITY_HANDLE_OWNING,
      std::make_shared<skity::PathBuilder>());
}

void skity_path_builder_destroy(skity_path_builder builder) {
  skity::capi::destroy_handle<skity_path_builder_s>(
      builder, SKITY_OBJECT_TYPE_PATH_BUILDER);
}

skity_path_builder skity_path_builder_create_from_path(skity_path source) {
  auto* p = path_of(source);
  if (!p) return nullptr;
  return skity::capi::alloc_handle<skity_path_builder_s>(
      SKITY_OBJECT_TYPE_PATH_BUILDER, SKITY_HANDLE_OWNING,
      std::make_shared<skity::PathBuilder>(*p));
}

skity_path_builder skity_path_builder_create_from_path_move(skity_path source) {
  auto* p = path_of(source, true);
  if (!p) return nullptr;
  // Allocate the fallible handle before consuming source geometry.
  auto* result = skity::capi::alloc_handle<skity_path_builder_s>(
      SKITY_OBJECT_TYPE_PATH_BUILDER, SKITY_HANDLE_OWNING,
      std::shared_ptr<skity::PathBuilder>{});
  if (result)
    result->impl = std::make_shared<skity::PathBuilder>(std::move(*p));
  return result;
}

skity_path skity_path_builder_snapshot(skity_path_builder builder) {
  auto* b = builder_of(builder);
  if (!b) return nullptr;
  return skity::capi::alloc_handle<skity_path_s>(
      SKITY_OBJECT_TYPE_PATH, SKITY_HANDLE_OWNING,
      std::make_shared<skity::Path>(b->Snapshot()));
}

skity_path skity_path_builder_detach(skity_path_builder builder) {
  auto* b = builder_of(builder);
  if (!b) return nullptr;
  // A failed handle allocation must leave builder intact.
  auto* result = skity::capi::alloc_handle<skity_path_s>(
      SKITY_OBJECT_TYPE_PATH, SKITY_HANDLE_OWNING,
      std::shared_ptr<skity::Path>{});
  if (result) result->impl = std::make_shared<skity::Path>(b->Detach());
  return result;
}

uint32_t skity_path_builder_detach_to(skity_path_builder builder,
                                      skity_path destination) {
  auto* b = builder_of(builder);
  auto* p = path_of(destination, true);
  if (!b || !p) return 0;
  b->DetachTo(p);
  return 1;
}

void skity_path_builder_move_to(skity_path_builder builder, float x, float y) {
  auto* b = builder_of(builder);
  if (b) b->MoveTo(x, y);
}

void skity_path_builder_line_to(skity_path_builder builder, float x, float y) {
  auto* b = builder_of(builder);
  if (b) b->LineTo(x, y);
}

void skity_path_builder_quad_to(skity_path_builder builder, float x1, float y1,
                                float x2, float y2) {
  auto* b = builder_of(builder);
  if (b) b->QuadTo(x1, y1, x2, y2);
}

void skity_path_builder_conic_to(skity_path_builder builder, float x1, float y1,
                                 float x2, float y2, float weight) {
  auto* b = builder_of(builder);
  if (b) b->ConicTo(x1, y1, x2, y2, weight);
}

void skity_path_builder_cubic_to(skity_path_builder builder, float x1, float y1,
                                 float x2, float y2, float x3, float y3) {
  auto* b = builder_of(builder);
  if (b) b->CubicTo(x1, y1, x2, y2, x3, y3);
}

void skity_path_builder_arc_to(skity_path_builder builder, float x1, float y1,
                               float x2, float y2, float radius) {
  auto* b = builder_of(builder);
  if (b) b->ArcTo(x1, y1, x2, y2, radius);
}

void skity_path_builder_arc_to_oval(skity_path_builder builder,
                                    const skity_rect* oval, float start_angle,
                                    float sweep_angle, uint32_t force_move_to) {
  auto* b = builder_of(builder);
  if (b && oval)
    b->ArcTo(to_rect(*oval), start_angle, sweep_angle, force_move_to != 0);
}

void skity_path_builder_arc_to_svg(skity_path_builder builder, float rx,
                                   float ry, float rotation,
                                   skity_arc_size large_arc,
                                   skity_path_direction sweep, float x,
                                   float y) {
  auto* b = builder_of(builder);
  if (b && valid_dir(sweep) &&
      (large_arc == SKITY_ARC_SIZE_SMALL || large_arc == SKITY_ARC_SIZE_LARGE))
    b->ArcTo(rx, ry, rotation, static_cast<skity::Path::ArcSize>(large_arc),
             to_dir(sweep), x, y);
}

void skity_path_builder_close(skity_path_builder builder) {
  auto* b = builder_of(builder);
  if (b) b->Close();
}

void skity_path_builder_reset(skity_path_builder builder) {
  auto* b = builder_of(builder);
  if (b) b->Reset();
}

void skity_path_builder_reserve(skity_path_builder builder, size_t point_count,
                                size_t verb_count, size_t conic_weight_count) {
  auto* b = builder_of(builder);
  if (b) b->Reserve(point_count, verb_count, conic_weight_count);
}

void skity_path_builder_set_fill_type(skity_path_builder builder,
                                      skity_path_fill_type type) {
  auto* b = builder_of(builder);
  if (b && (type == SKITY_PATH_FILL_TYPE_WINDING ||
            type == SKITY_PATH_FILL_TYPE_EVEN_ODD))
    b->SetFillType(static_cast<skity::Path::PathFillType>(type));
}

void skity_path_builder_set_last_pt(skity_path_builder builder, float x,
                                    float y) {
  auto* b = builder_of(builder);
  if (b) b->SetLastPt(x, y);
}

void skity_path_builder_add_circle(skity_path_builder builder, float x, float y,
                                   float radius, skity_path_direction dir) {
  auto* b = builder_of(builder);
  if (b && valid_dir(dir)) b->AddCircle(x, y, radius, to_dir(dir));
}

void skity_path_builder_add_rect(skity_path_builder builder,
                                 const skity_rect* rect,
                                 skity_path_direction dir) {
  auto* b = builder_of(builder);
  if (b && rect && valid_dir(dir)) b->AddRect(to_rect(*rect), to_dir(dir));
}

void skity_path_builder_add_rect_start(skity_path_builder builder,
                                       const skity_rect* rect,
                                       skity_path_direction dir,
                                       uint32_t start_index) {
  auto* b = builder_of(builder);
  if (b && rect && valid_dir(dir))
    b->AddRect(to_rect(*rect), to_dir(dir), start_index);
}

void skity_path_builder_add_oval(skity_path_builder builder,
                                 const skity_rect* oval,
                                 skity_path_direction dir) {
  auto* b = builder_of(builder);
  if (b && oval && valid_dir(dir)) b->AddOval(to_rect(*oval), to_dir(dir));
}

void skity_path_builder_add_oval_start(skity_path_builder builder,
                                       const skity_rect* oval,
                                       skity_path_direction dir,
                                       uint32_t start_index) {
  auto* b = builder_of(builder);
  if (b && oval && valid_dir(dir))
    b->AddOval(to_rect(*oval), to_dir(dir), start_index);
}

void skity_path_builder_add_round_rect(skity_path_builder builder,
                                       const skity_rect* rect, float rx,
                                       float ry, skity_path_direction dir) {
  auto* b = builder_of(builder);
  if (b && rect && valid_dir(dir))
    b->AddRoundRect(to_rect(*rect), rx, ry, to_dir(dir));
}

void skity_path_builder_add_round_rect_radii(skity_path_builder builder,
                                             const skity_rect* rect,
                                             const float radii[8],
                                             skity_path_direction dir) {
  auto* b = builder_of(builder);
  if (b && rect && radii && valid_dir(dir))
    b->AddRoundRect(to_rect(*rect), radii, to_dir(dir));
}

void skity_path_builder_add_rrect(skity_path_builder builder,
                                  const skity_rect* rect,
                                  const skity_vec2 radii[4],
                                  skity_path_direction dir) {
  auto* b = builder_of(builder);
  if (b && rect && radii && valid_dir(dir))
    b->AddRRect(to_rrect(*rect, radii), to_dir(dir));
}

void skity_path_builder_add_rrect_start(skity_path_builder builder,
                                        const skity_rect* rect,
                                        const skity_vec2 radii[4],
                                        skity_path_direction dir,
                                        uint32_t start_index) {
  auto* b = builder_of(builder);
  if (b && rect && radii && valid_dir(dir))
    b->AddRRect(to_rrect(*rect, radii), to_dir(dir), start_index);
}

void skity_path_builder_add_path(skity_path_builder builder, skity_path source,
                                 float dx, float dy, skity_path_add_mode mode) {
  auto* b = builder_of(builder);
  auto* p = path_of(source);
  if (b && p && valid_mode(mode))
    b->AddPath(*p, dx, dy, static_cast<skity::Path::AddMode>(mode));
}

void skity_path_builder_add_path_matrix(skity_path_builder builder,
                                        skity_path source,
                                        const skity_matrix* matrix,
                                        skity_path_add_mode mode) {
  auto* b = builder_of(builder);
  auto* p = path_of(source);
  if (!b || !p || !matrix || !valid_mode(mode)) return;
  const float* m = matrix->m;
  skity::Matrix value{m[0], m[1], m[2],  m[3],  m[4],  m[5],  m[6],  m[7],
                      m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]};
  b->AddPath(*p, value, static_cast<skity::Path::AddMode>(mode));
}

void skity_path_builder_reverse_add_path(skity_path_builder builder,
                                         skity_path source) {
  auto* b = builder_of(builder);
  auto* p = path_of(source);
  if (b && p) b->ReverseAddPath(*p);
}

void skity_path_builder_reverse_path_to(skity_path_builder builder,
                                        skity_path source) {
  auto* b = builder_of(builder);
  auto* p = path_of(source);
  if (b && p) b->ReversePathTo(*p);
}

void skity_path_builder_reverse_builder_to(skity_path_builder builder,
                                           skity_path_builder source) {
  auto* b = builder_of(builder);
  auto* s = builder_of(source);
  if (b && s) b->ReversePathTo(*s);
}

void skity_path_builder_swap(skity_path_builder builder,
                             skity_path_builder other) {
  auto* b = builder_of(builder);
  auto* o = builder_of(other);
  if (b && o) b->Swap(*o);
}

uint32_t skity_path_builder_is_empty(skity_path_builder builder) {
  auto* b = builder_of(builder);
  return b && b->IsEmpty() ? 1u : 0u;
}

size_t skity_path_builder_count_points(skity_path_builder builder) {
  auto* b = builder_of(builder);
  return b ? b->CountPoints() : 0;
}

size_t skity_path_builder_count_verbs(skity_path_builder builder) {
  auto* b = builder_of(builder);
  return b ? b->CountVerbs() : 0;
}

skity_path_fill_type skity_path_builder_get_fill_type(
    skity_path_builder builder) {
  auto* b = builder_of(builder);
  return b ? static_cast<skity_path_fill_type>(b->GetFillType())
           : SKITY_PATH_FILL_TYPE_WINDING;
}

uint32_t skity_path_builder_get_point(skity_path_builder builder, int32_t index,
                                      skity_vec2* out) {
  auto* b = builder_of(builder);
  if (!b || !out || index < 0 || static_cast<size_t>(index) >= b->CountPoints())
    return 0;
  auto point = b->GetPoint(index);
  *out = {point.x, point.y};
  return 1;
}

uint32_t skity_path_builder_get_last_pt(skity_path_builder builder,
                                        skity_vec2* out) {
  auto* b = builder_of(builder);
  skity::Vec2 point;
  if (!b || !out || !b->GetLastPt(&point)) return 0;
  *out = {point.x, point.y};
  return 1;
}

uint32_t skity_path_builder_get_last_move_pt(skity_path_builder builder,
                                             skity_vec2* out) {
  auto* b = builder_of(builder);
  if (!b || !out || b->IsEmpty()) return 0;
  auto point = b->GetLastMovePt();
  *out = {point.x, point.y};
  return 1;
}

uint32_t skity_path_builder_get_bounds(skity_path_builder builder,
                                       skity_rect* out) {
  auto* b = builder_of(builder);
  if (!b || !out) return 0;
  auto bounds = b->GetBounds();
  *out = {bounds.Left(), bounds.Top(), bounds.Right(), bounds.Bottom()};
  return 1;
}

}  // extern "C"
