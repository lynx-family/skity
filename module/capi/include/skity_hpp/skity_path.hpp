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

  /** Arc-size selector for ArcTo SVG (which of the two candidate arcs). */
  enum class ArcSize : uint32_t {
    kSmall = SKITY_ARC_SIZE_SMALL,
    kLarge = SKITY_ARC_SIZE_LARGE,
  };

  /** How AddPath splices @p src into the destination. */
  enum class AddMode : uint32_t {
    kAppend = SKITY_PATH_ADD_MODE_APPEND,
    kExtend = SKITY_PATH_ADD_MODE_EXTEND,
  };

  /** Verb kinds, in storage order. */
  enum class Verb : uint32_t {
    kMove = SKITY_PATH_VERB_MOVE,
    kLine = SKITY_PATH_VERB_LINE,
    kQuad = SKITY_PATH_VERB_QUAD,
    kConic = SKITY_PATH_VERB_CONIC,
    kCubic = SKITY_PATH_VERB_CUBIC,
    kClose = SKITY_PATH_VERB_CLOSE,
    kDone = SKITY_PATH_VERB_DONE,
  };

  /** Convexity classification (kUnknown = not yet computed). */
  enum class ConvexityType : uint32_t {
    kUnknown = SKITY_CONVEXITY_UNKNOWN,
    kConvex = SKITY_CONVEXITY_CONVEX,
    kConcave = SKITY_CONVEXITY_CONCAVE,
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
  /** Tangent arc: line/curve into a circle of @p radius tangent to both
   *  segments (@p x1,@p y1)-(@p x2,@p y2) and (@p x2,@p y2)-(@p x3,@p y3). */
  Path& ArcTo(float x1, float y1, float x2, float y2, float radius) {
    skity_path_arc_to(get(), x1, y1, x2, y2, radius);
    return *this;
  }
  /** Arc of the oval bounded by @p oval, from @p start_angle sweeping
   *  @p sweep_angle degrees; @p force_move_to starts a new contour. */
  Path& ArcTo(const Rect& oval, float start_angle, float sweep_angle,
              bool force_move_to) {
    skity_path_arc_to_oval(get(), &oval, start_angle, sweep_angle,
                           force_move_to ? 1u : 0u);
    return *this;
  }
  /** SVG-style elliptical arc from the last point to (@p x, @p y). */
  Path& ArcTo(float rx, float ry, float x_axis_rotate, ArcSize large_arc,
              Direction sweep, float x, float y) {
    skity_path_arc_to_svg(get(), rx, ry, x_axis_rotate,
                          static_cast<skity_arc_size>(large_arc),
                          static_cast<skity_path_direction>(sweep), x, y);
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
  /** Rounded rect with per-corner radii: @p radii holds four (x, y) pairs,
   *  order TL, TR, BR, BL. */
  Path& AddRoundRect(const Rect& rect, const Vec2 radii[4],
                     Direction dir = Direction::kCW) {
    skity_path_add_round_rect_radii(get(), &rect, &radii[0].e[0],
                                    static_cast<skity_path_direction>(dir));
    return *this;
  }
  Path& AddRRect(const Rect& rect, float rx, float ry,
                 Direction dir = Direction::kCW) {
    skity_path_add_rrect(get(), &rect, rx, ry,
                         static_cast<skity_path_direction>(dir));
    return *this;
  }
  /** Rounded-rect value overload (legacy shape). */
  Path& AddRRect(const RRect& rrect, Direction dir = Direction::kCW) {
    return AddRoundRect(rrect.GetBounds(), rrect.GetRadii(), dir);
  }
  Path& AddPath(const Path& src, float dx = 0.f, float dy = 0.f) {
    skity_path_add_path(get(), src.get(), dx, dy);
    return *this;
  }
  Path& AddPath(const Path& src, AddMode mode) {
    skity_path_add_path_with_mode(get(), src.get(),
                                  static_cast<skity_path_add_mode>(mode));
    return *this;
  }
  Path& AddPathMatrix(const Path& src, const Matrix& matrix) {
    skity_path_add_path_matrix(get(), src.get(), &matrix);
    return *this;
  }
  Path& AddPathMatrix(const Path& src, const Matrix& matrix, AddMode mode) {
    skity_path_add_path_matrix_with_mode(
        get(), src.get(), &matrix, static_cast<skity_path_add_mode>(mode));
    return *this;
  }
  /** Append the contours of @p src in reverse order. */
  Path& ReverseAddPath(const Path& src) {
    skity_path_reverse_add_path(get(), src.get());
    return *this;
  }

  void Transform(const Matrix& matrix) { skity_path_transform(get(), &matrix); }

  /** Transformed copy of this path. */
  Path CopyWithMatrix(const Matrix& matrix) const {
    return Path(skity_path_copy_with_matrix(get(), &matrix));
  }

  /** Scaled copy of this path. */
  Path CopyWithScale(float scale) const {
    return Path(skity_path_copy_with_scale(get(), scale));
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

  /** Point at @p index (Vec4, as legacy GetPoint). */
  Point GetPoint(int32_t index) const {
    Point out{};
    skity_path_get_point(get(), index, &out);
    return out;
  }
  Verb GetVerb(int32_t index) const {
    return static_cast<Verb>(skity_path_get_verb(get(), index));
  }
  /** Conic weight at @p index; 0 when @p index is out of range. */
  float GetConicWeight(int32_t index) const {
    float w = 0.f;
    skity_path_get_conic_weight(get(), index, &w);
    return w;
  }

  /** Bitmask of the segment kinds present (1<<verb for line/quad/conic/
   *  cubic), matching the legacy GetSegmentMasks. */
  uint32_t GetSegmentMasks() const {
    return skity_path_get_segment_masks(get());
  }

  /** True when the path is exactly a rect; @p out_rect (optional) receives it.
   */
  bool IsRect(Rect* out_rect = nullptr) const {
    return skity_path_is_rect(get(), out_rect, nullptr) != 0;
  }
  /** True when the path is exactly one line; @p out_pts (optional, 2 Vec4)
   *  receives the end points. */
  bool IsLine(Point out_pts[2] = nullptr) const {
    return skity_path_is_line(get(), out_pts) != 0;
  }
  bool IsEmpty() const { return skity_path_is_empty(get()) != 0; }
  bool IsFinite() const { return skity_path_is_finite(get()) != 0; }
  bool IsEqual(const Path& other) const {
    return skity_path_is_equal(get(), other.get()) != 0;
  }

  ConvexityType GetConvexityType() const {
    return static_cast<ConvexityType>(skity_path_get_convexity_type(get()));
  }
  /** Pin the convexity (skip the lazy computation / force a classification). */
  void SetConvexityType(ConvexityType type) {
    skity_path_set_convexity_type(get(),
                                  static_cast<skity_convexity_type>(type));
  }

  /** Last point of the path; false when the path has no points yet. */
  bool GetLastPt(Point* out) const {
    return skity_path_get_last_pt(get(), out) != 0;
  }
  void SetLastPt(float x, float y) { skity_path_set_last_pt(get(), x, y); }
  /** Start point of the current contour (legacy GetLastMovePt). */
  void GetLastMovePt(Point* out) const {
    skity_path_get_last_move_pt(get(), out);
  }

 private:
  explicit Path(skity_path h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_HPP
