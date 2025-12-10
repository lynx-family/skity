// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_PATH_VISITOR_HPP
#define SRC_GRAPHIC_PATH_VISITOR_HPP

#include <skity/geometry/matrix.hpp>
#include <skity/graphic/path.hpp>

namespace skity {

/**
 * A abstract class to do common path processing.
 *
 */
class PathVisitor {
 public:
  /**
   * Construct the PathVisitor instance.
   *
   * @param approx_curve  If set to true, all curve will be approximated by
   *                      straight lines. And only OnLineTo will be called to
   *                      subclass.
   *                      Otherwise, all cubic-curve and conic-curve will be
   *                      approximated by quad-curve, and OnQuadTo will be
   *                      called on subclass.
   */
  explicit PathVisitor(bool approx_curve, const Matrix& matrix)
      : approx_curve_(approx_curve), matrix_(matrix) {}
  virtual ~PathVisitor() = default;

  void VisitPath(Path const& path, bool force_close);

 private:
  void HandleMoveTo(Vec2 const& p);
  void HandleLineTo(Vec2 const& p1, Vec2 const& p2);
  void HandleQuadTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                    float precision);
  void HandleConicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                     float weight, float precision);
  void HandleCubicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                     Vec2 const& p4, float precision);
  void HandleClose();

 protected:
  Path::PathFillType GetFillType() const { return fill_type_; }

  virtual void OnBeginPath() = 0;

  virtual void OnEndPath() = 0;

  virtual void OnMoveTo(Vec2 const& p) = 0;

  virtual void OnLineTo(Vec2 const& p1, Vec2 const& p2) = 0;

  virtual void OnQuadTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3) = 0;

  virtual void OnConicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                         float weight) = 0;

  virtual void OnCubicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                         Vec2 const& p4) = 0;

  virtual void OnClose() = 0;

 private:
  bool approx_curve_;
  Path::PathFillType fill_type_ = Path::PathFillType::kWinding;
  Vec2 prev_pt_ = {};
  Matrix matrix_ = {};
};

}  // namespace skity

#endif  // SRC_GRAPHIC_PATH_VISITOR_HPP
