// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_PATH_BUILDER_HPP
#define INCLUDE_SKITY_GRAPHIC_PATH_BUILDER_HPP

#include <cstddef>
#include <skity/graphic/path.hpp>

namespace skity {

/**
 * @brief Builds an independently owned Path using two-dimensional coordinates.
 *        Existing Path editing APIs remain available during migration.
 */
class SKITY_API PathBuilder {
 public:
  PathBuilder() = default;
  /** @brief Copies src for editing without changing the source. */
  explicit PathBuilder(const Path& src);
  /** @brief Transfers src's contents and resets src to an empty winding Path.
   */
  explicit PathBuilder(Path&& src);

  PathBuilder(const PathBuilder&) = delete;
  PathBuilder& operator=(const PathBuilder&) = delete;
  PathBuilder(PathBuilder&& other) noexcept;
  PathBuilder& operator=(PathBuilder&& other) noexcept;

  PathBuilder& MoveTo(float x, float y) {
    path_.MoveTo(x, y);
    return *this;
  }

  PathBuilder& MoveTo(Vec2 point) {
    path_.MoveTo(point.x, point.y);
    return *this;
  }

  PathBuilder& LineTo(float x, float y) {
    path_.LineTo(x, y);
    return *this;
  }

  PathBuilder& LineTo(Vec2 point) {
    path_.LineTo(point.x, point.y);
    return *this;
  }

  PathBuilder& QuadTo(float x1, float y1, float x2, float y2) {
    path_.QuadTo(x1, y1, x2, y2);
    return *this;
  }

  PathBuilder& QuadTo(Vec2 control, Vec2 end) {
    path_.QuadTo(control.x, control.y, end.x, end.y);
    return *this;
  }

  PathBuilder& ConicTo(float x1, float y1, float x2, float y2, float weight) {
    path_.ConicTo(x1, y1, x2, y2, weight);
    return *this;
  }

  PathBuilder& ConicTo(Vec2 control, Vec2 end, float weight) {
    path_.ConicTo(control.x, control.y, end.x, end.y, weight);
    return *this;
  }

  PathBuilder& CubicTo(float x1, float y1, float x2, float y2, float x3,
                       float y3) {
    path_.CubicTo(x1, y1, x2, y2, x3, y3);
    return *this;
  }

  PathBuilder& CubicTo(Vec2 control1, Vec2 control2, Vec2 end) {
    path_.CubicTo(control1.x, control1.y, control2.x, control2.y, end.x, end.y);
    return *this;
  }

  PathBuilder& ArcTo(float x1, float y1, float x2, float y2, float radius) {
    path_.ArcTo(x1, y1, x2, y2, radius);
    return *this;
  }

  PathBuilder& ArcTo(const Rect& oval, float start_angle, float sweep_angle,
                     bool force_move) {
    path_.ArcTo(oval, start_angle, sweep_angle, force_move);
    return *this;
  }

  PathBuilder& ArcTo(float rx, float ry, float rotation,
                     Path::ArcSize large_arc, Path::Direction sweep, float x,
                     float y) {
    path_.ArcTo(rx, ry, rotation, large_arc, sweep, x, y);
    return *this;
  }

  PathBuilder& Close() {
    path_.Close();
    return *this;
  }

  PathBuilder& AddCircle(float x, float y, float radius,
                         Path::Direction dir = Path::Direction::kCW) {
    path_.AddCircle(x, y, radius, dir);
    return *this;
  }

  PathBuilder& AddOval(const Rect& oval,
                       Path::Direction dir = Path::Direction::kCW) {
    path_.AddOval(oval, dir);
    return *this;
  }

  PathBuilder& AddOval(const Rect& oval, Path::Direction dir, uint32_t start) {
    path_.AddOval(oval, dir, start);
    return *this;
  }

  PathBuilder& AddRect(const Rect& rect,
                       Path::Direction dir = Path::Direction::kCW,
                       uint32_t start = 0) {
    path_.AddRect(rect, dir, start);
    return *this;
  }

  PathBuilder& AddRect(float left, float top, float right, float bottom,
                       Path::Direction dir = Path::Direction::kCW) {
    path_.AddRect(left, top, right, bottom, dir);
    return *this;
  }

  PathBuilder& AddRoundRect(const Rect& rect, float rx, float ry,
                            Path::Direction dir = Path::Direction::kCW) {
    path_.AddRoundRect(rect, rx, ry, dir);
    return *this;
  }

  PathBuilder& AddRoundRect(const Rect& rect, const float radii[],
                            Path::Direction dir = Path::Direction::kCW) {
    path_.AddRoundRect(rect, radii, dir);
    return *this;
  }

  PathBuilder& AddRRect(const RRect& rrect,
                        Path::Direction dir = Path::Direction::kCW) {
    path_.AddRRect(rrect, dir);
    return *this;
  }

  PathBuilder& AddRRect(const RRect& rrect, Path::Direction dir,
                        uint32_t start) {
    path_.AddRRect(rrect, dir, start);
    return *this;
  }

  PathBuilder& AddPath(const Path& src,
                       Path::AddMode mode = Path::AddMode::kAppend) {
    path_.AddPath(src, mode);
    return *this;
  }

  PathBuilder& AddPath(const Path& src, float dx, float dy,
                       Path::AddMode mode = Path::AddMode::kAppend) {
    path_.AddPath(src, dx, dy, mode);
    return *this;
  }

  PathBuilder& AddPath(const Path& src, const Matrix& matrix,
                       Path::AddMode mode = Path::AddMode::kAppend) {
    path_.AddPath(src, matrix, mode);
    return *this;
  }

  PathBuilder& ReversePathTo(const Path& src) {
    path_.ReversePathTo(src);
    return *this;
  }

  PathBuilder& SetLastPt(float x, float y) {
    path_.SetLastPt(x, y);
    return *this;
  }

  PathBuilder& SetLastPt(Vec2 point) {
    path_.SetLastPt(point.x, point.y);
    return *this;
  }

  PathBuilder& SetFillType(Path::PathFillType type) {
    path_.SetFillType(type);
    return *this;
  }

  PathBuilder& ReverseAddPath(const Path& src) {
    if (!src.IsEmpty()) {
      path_.ReverseAddPath(src);
    }
    return *this;
  }
  PathBuilder& ReversePathTo(const PathBuilder& src);

  /** @brief Clears geometry and restores winding fill, retaining capacity. */
  PathBuilder& Reset() {
    path_.Reset();
    return *this;
  }
  /** @brief Reserves total array capacities without changing geometry. */
  void Reserve(size_t point_count, size_t verb_count,
               size_t conic_weight_count = 0);

  bool IsEmpty() const { return path_.IsEmpty(); }
  size_t CountPoints() const { return path_.CountPoints(); }
  size_t CountVerbs() const { return path_.CountVerbs(); }
  Path::PathFillType GetFillType() const { return path_.GetFillType(); }
  Vec2 GetPoint(int index) const { return path_.GetPoint2D(index); }
  bool GetLastPt(Vec2* point) const { return path_.GetLastPt2D(point); }
  Vec2 GetLastMovePt() const { return path_.GetLastMovePt2D(); }
  Rect GetBounds() const { return path_.GetBounds(); }

  /** @brief Returns an independent copy while retaining this builder's content.
   */
  Path Snapshot() const;
  /**
   * @brief Transfers geometry to a Path without copying arrays, then resets
   *        this builder to empty winding. Transferred capacity is not retained.
   * @note Initialize a new Path with the result, or use DetachTo() to replace
   *       an existing Path. Assigning the result to an existing Path uses
   *       Path's copy assignment and copies geometry.
   */
  Path Detach();
  /**
   * @brief Replaces dst's complete value without copying geometry and reuses
   *        its old capacity in this empty winding builder. A null dst is a
   * no-op.
   */
  void DetachTo(Path* dst);
  void Swap(PathBuilder& other) noexcept;

 private:
  Path path_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_PATH_BUILDER_HPP
