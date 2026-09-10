// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_HPP

// Path wrapper of the header-only RAII layer: contours built verb by verb,
// or from simple shapes. See skity.hpp for the layer's design.

#include <skity_c/skity_path.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_types.hpp>

namespace skity {
namespace raii {

/** Path wrapper: contours built verb by verb, or from simple shapes. */
class Path : public detail::OwnHandle<skity_path, skity_path_destroy> {
 public:
  enum class PathFillType : uint32_t {
    kWinding = SKITY_PATH_FILL_TYPE_WINDING,
    kEvenOdd = SKITY_PATH_FILL_TYPE_EVEN_ODD,
  };

  enum class Direction : uint32_t {
    kCW = SKITY_PATH_DIRECTION_CW,
    kCCW = SKITY_PATH_DIRECTION_CCW,
  };

  Path() : OwnHandle(skity_path_create()) {}

  /** Deep copy (the C++ Path was copyable; owning handles are not). */
  Path Clone() const { return Path(skity_path_clone(get())); }

  void Reset() { skity_path_reset(get()); }

  void SetFillType(PathFillType type) {
    skity_path_set_fill_type(get(), static_cast<skity_path_fill_type>(type));
  }
  PathFillType GetFillType() const {
    return static_cast<PathFillType>(skity_path_get_fill_type(get()));
  }

  Path& MoveTo(float x, float y) {
    skity_path_move_to(get(), x, y);
    return *this;
  }
  Path& LineTo(float x, float y) {
    skity_path_line_to(get(), x, y);
    return *this;
  }
  Path& QuadTo(float x1, float y1, float x2, float y2) {
    skity_path_quad_to(get(), x1, y1, x2, y2);
    return *this;
  }
  Path& ConicTo(float x1, float y1, float x2, float y2, float weight) {
    skity_path_conic_to(get(), x1, y1, x2, y2, weight);
    return *this;
  }
  Path& CubicTo(float x1, float y1, float x2, float y2, float x3, float y3) {
    skity_path_cubic_to(get(), x1, y1, x2, y2, x3, y3);
    return *this;
  }
  Path& ArcTo(float x1, float y1, float x2, float y2, float radius) {
    skity_path_arc_to(get(), x1, y1, x2, y2, radius);
    return *this;
  }
  Path& Close() {
    skity_path_close(get());
    return *this;
  }

  Path& AddRect(const Rect& rect, Direction dir = Direction::kCW) {
    skity_path_add_rect(get(), &rect, static_cast<skity_path_direction>(dir));
    return *this;
  }
  Path& AddOval(const Rect& oval, Direction dir = Direction::kCW) {
    skity_path_add_oval(get(), &oval, static_cast<skity_path_direction>(dir));
    return *this;
  }
  Path& AddCircle(float x, float y, float radius,
                  Direction dir = Direction::kCW) {
    skity_path_add_circle(get(), x, y, radius,
                          static_cast<skity_path_direction>(dir));
    return *this;
  }
  Path& AddRoundRect(const Rect& rect, float rx, float ry,
                     Direction dir = Direction::kCW) {
    skity_path_add_round_rect(get(), &rect, rx, ry,
                              static_cast<skity_path_direction>(dir));
    return *this;
  }
  Path& AddPath(const Path& src, float dx = 0.f, float dy = 0.f) {
    skity_path_add_path(get(), src.get(), dx, dy);
    return *this;
  }
  Path& AddPathMatrix(const Path& src, const Matrix& matrix) {
    skity_path_add_path_matrix(get(), src.get(), &matrix);
    return *this;
  }

  void Transform(const Matrix& matrix) { skity_path_transform(get(), &matrix); }

  /** Transformed copy of this path. */
  Path CopyWithMatrix(const Matrix& matrix) const {
    return Path(skity_path_copy_with_matrix(get(), &matrix));
  }

  Rect GetBounds() const {
    Rect out;
    skity_path_get_bounds(get(), &out);
    return out;
  }

  bool Contains(float x, float y) const {
    return skity_path_contains(get(), x, y) != 0;
  }

  uint32_t CountVerbs() const { return skity_path_count_verbs(get()); }
  uint32_t CountPoints() const { return skity_path_count_points(get()); }

  bool IsEmpty() const { return skity_path_is_empty(get()) != 0; }
  bool IsFinite() const { return skity_path_is_finite(get()) != 0; }

 private:
  explicit Path(skity_path h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_HPP
