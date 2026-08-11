// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_path.h>

#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/path.hpp>

#include "handle.hpp"

namespace {

skity::Path* path_of(skity_path handle) {
  auto* w = skity::capi::resolve<skity_path_s>(handle, SKITY_OBJECT_TYPE_PATH);
  return w ? static_cast<skity::Path*>(w->impl.get()) : nullptr;
}

skity::Path::Direction to_dir(skity_path_direction d) {
  return static_cast<skity::Path::Direction>(d);
}

}  // namespace

extern "C" {

skity_path skity_path_create(void) {
  return skity::capi::alloc_handle<skity_path_s>(
      SKITY_OBJECT_TYPE_PATH, SKITY_HANDLE_OWNING,
      std::make_shared<skity::Path>());
}

void skity_path_destroy(skity_path path) {
  skity::capi::destroy_handle<skity_path_s>(path, SKITY_OBJECT_TYPE_PATH);
}

void skity_path_reset(skity_path path) {
  if (auto* p = path_of(path)) p->Reset();
}

void skity_path_set_fill_type(skity_path path, skity_path_fill_type type) {
  if (auto* p = path_of(path))
    p->SetFillType(static_cast<skity::Path::PathFillType>(type));
}

void skity_path_move_to(skity_path path, float x, float y) {
  if (auto* p = path_of(path)) p->MoveTo(x, y);
}

void skity_path_line_to(skity_path path, float x, float y) {
  if (auto* p = path_of(path)) p->LineTo(x, y);
}

void skity_path_quad_to(skity_path path, float x1, float y1, float x2,
                        float y2) {
  if (auto* p = path_of(path)) p->QuadTo(x1, y1, x2, y2);
}

void skity_path_conic_to(skity_path path, float x1, float y1, float x2,
                         float y2, float weight) {
  if (auto* p = path_of(path)) p->ConicTo(x1, y1, x2, y2, weight);
}

void skity_path_cubic_to(skity_path path, float x1, float y1, float x2,
                         float y2, float x3, float y3) {
  if (auto* p = path_of(path)) p->CubicTo(x1, y1, x2, y2, x3, y3);
}

void skity_path_arc_to(skity_path path, float x1, float y1, float x2, float y2,
                       float radius) {
  if (auto* p = path_of(path)) p->ArcTo(x1, y1, x2, y2, radius);
}

void skity_path_close(skity_path path) {
  if (auto* p = path_of(path)) p->Close();
}

void skity_path_add_circle(skity_path path, float x, float y, float radius,
                           skity_path_direction dir) {
  if (auto* p = path_of(path)) p->AddCircle(x, y, radius, to_dir(dir));
}

void skity_path_add_oval(skity_path path, const skity_rect* oval,
                         skity_path_direction dir) {
  auto* p = path_of(path);
  if (p != nullptr && oval != nullptr) {
    p->AddOval(*reinterpret_cast<const skity::Rect*>(oval), to_dir(dir));
  }
}

void skity_path_add_rect(skity_path path, const skity_rect* rect,
                         skity_path_direction dir) {
  auto* p = path_of(path);
  if (p != nullptr && rect != nullptr) {
    p->AddRect(*reinterpret_cast<const skity::Rect*>(rect), to_dir(dir));
  }
}

void skity_path_add_round_rect(skity_path path, const skity_rect* rect,
                               float rx, float ry, skity_path_direction dir) {
  auto* p = path_of(path);
  if (p != nullptr && rect != nullptr) {
    p->AddRoundRect(*reinterpret_cast<const skity::Rect*>(rect), rx, ry,
                    to_dir(dir));
  }
}

void skity_path_transform(skity_path path, const skity_matrix* matrix) {
  auto* p = path_of(path);
  if (p != nullptr && matrix != nullptr) {
    *p = p->CopyWithMatrix(*reinterpret_cast<const skity::Matrix*>(matrix));
  }
}

void skity_path_get_bounds(skity_path path, skity_rect* out_bounds) {
  auto* p = path_of(path);
  if (p == nullptr || out_bounds == nullptr) return;
  skity::Rect b = p->GetBounds();
  out_bounds->left = b.Left();
  out_bounds->top = b.Top();
  out_bounds->right = b.Right();
  out_bounds->bottom = b.Bottom();
}

void skity_path_add_path(skity_path path, skity_path src, float dx, float dy) {
  auto* p = path_of(path);
  auto* s = path_of(src);
  if (p != nullptr && s != nullptr) {
    p->AddPath(*s, dx, dy);
  }
}

void skity_path_add_rrect(skity_path path, const skity_rect* rect, float rx,
                          float ry, skity_path_direction dir) {
  auto* p = path_of(path);
  if (p != nullptr && rect != nullptr) {
    p->AddRoundRect(*reinterpret_cast<const skity::Rect*>(rect), rx, ry,
                    to_dir(dir));
  }
}

uint32_t skity_path_contains(skity_path path, float x, float y) {
  auto* p = path_of(path);
  return (p != nullptr && p->Contains(x, y)) ? 1u : 0u;
}

skity_path_fill_type skity_path_get_fill_type(skity_path path) {
  auto* p = path_of(path);
  return p ? static_cast<skity_path_fill_type>(p->GetFillType())
           : SKITY_PATH_FILL_TYPE_WINDING;
}

void skity_path_add_path_matrix(skity_path path, skity_path src,
                                const skity_matrix* matrix) {
  auto* p = path_of(path);
  auto* s = path_of(src);
  if (p != nullptr && s != nullptr && matrix != nullptr) {
    p->AddPath(*s, *reinterpret_cast<const skity::Matrix*>(matrix));
  }
}

void skity_path_reverse_add_path(skity_path path, skity_path src) {
  auto* p = path_of(path);
  auto* s = path_of(src);
  if (p != nullptr && s != nullptr) {
    p->ReverseAddPath(*s);
  }
}

uint32_t skity_path_count_verbs(skity_path path) {
  auto* p = path_of(path);
  return p ? (uint32_t)p->CountVerbs() : 0u;
}

uint32_t skity_path_count_points(skity_path path) {
  auto* p = path_of(path);
  return p ? (uint32_t)p->CountPoints() : 0u;
}

uint32_t skity_path_get_point(skity_path path, int32_t index, skity_vec4* out) {
  auto* p = path_of(path);
  if (p == nullptr || out == nullptr || index < 0) return 0u;
  skity::Point pt = p->GetPoint(index);
  *reinterpret_cast<skity::Vec4*>(out) = pt;
  return 1u;
}

uint32_t skity_path_is_rect(skity_path path, skity_rect* out_rect,
                            uint32_t* out_is_closed) {
  auto* p = path_of(path);
  if (p == nullptr) return 0u;
  skity::Rect r;
  bool is_closed = false;
  bool ok = p->IsRect(&r, &is_closed);
  if (!ok) return 0u;
  if (out_rect != nullptr) {
    out_rect->left = r.Left();
    out_rect->top = r.Top();
    out_rect->right = r.Right();
    out_rect->bottom = r.Bottom();
  }
  if (out_is_closed != nullptr) *out_is_closed = is_closed ? 1u : 0u;
  return 1u;
}

void skity_path_arc_to_oval(skity_path path, const skity_rect* oval,
                            float start_angle, float sweep_angle,
                            uint32_t force_move_to) {
  auto* p = path_of(path);
  if (p != nullptr && oval != nullptr) {
    p->ArcTo(*reinterpret_cast<const skity::Rect*>(oval), start_angle,
             sweep_angle, force_move_to != 0u);
  }
}

void skity_path_arc_to_svg(skity_path path, float rx, float ry,
                           float x_axis_rotate, skity_arc_size large_arc,
                           skity_path_direction sweep, float x, float y) {
  auto* p = path_of(path);
  if (p == nullptr) return;
  p->ArcTo(rx, ry, x_axis_rotate, static_cast<skity::Path::ArcSize>(large_arc),
           to_dir(sweep), x, y);
}

void skity_path_add_round_rect_radii(skity_path path, const skity_rect* rect,
                                     const float* radii,
                                     skity_path_direction dir) {
  auto* p = path_of(path);
  if (p != nullptr && rect != nullptr && radii != nullptr) {
    p->AddRoundRect(*reinterpret_cast<const skity::Rect*>(rect), radii,
                    to_dir(dir));
  }
}

uint32_t skity_path_get_last_pt(skity_path path, skity_point* out) {
  auto* p = path_of(path);
  if (p == nullptr || out == nullptr) return 0u;
  skity::Point pt;
  if (!p->GetLastPt(&pt)) return 0u;
  *reinterpret_cast<skity::Vec4*>(out) = pt;
  return 1u;
}

void skity_path_set_last_pt(skity_path path, float x, float y) {
  if (auto* p = path_of(path)) p->SetLastPt(x, y);
}

skity_path skity_path_copy_with_matrix(skity_path path,
                                       const skity_matrix* matrix) {
  auto* p = path_of(path);
  if (p == nullptr || matrix == nullptr) return nullptr;
  skity::Path copy =
      p->CopyWithMatrix(*reinterpret_cast<const skity::Matrix*>(matrix));
  return skity::capi::alloc_handle<skity_path_s>(
      SKITY_OBJECT_TYPE_PATH, SKITY_HANDLE_OWNING,
      std::make_shared<skity::Path>(std::move(copy)));
}

skity_path skity_path_copy_with_scale(skity_path path, float scale) {
  auto* p = path_of(path);
  if (p == nullptr) return nullptr;
  skity::Path copy = p->CopyWithScale(scale);
  return skity::capi::alloc_handle<skity_path_s>(
      SKITY_OBJECT_TYPE_PATH, SKITY_HANDLE_OWNING,
      std::make_shared<skity::Path>(std::move(copy)));
}

}  // extern "C"
