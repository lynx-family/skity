// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_PATH_SCANNER_HPP
#define SRC_GRAPHIC_PATH_SCANNER_HPP

#include "src/graphic/path_visitor.hpp"

namespace skity {

class PathScanner : public PathVisitor {
 public:
  PathScanner(float x, float y);
  ~PathScanner() override = default;

  int32_t GetWindingCount() const { return winding_; }
  int32_t GetOnCurveCount() const { return on_curve_count_; }
  int32_t GetRayIntersectsVertexCount() const {
    return ray_intesects_vertex_count_;
  }

 protected:
  void OnBeginPath() override {}
  void OnEndPath() override {}
  void OnClose() override {}
  void OnMoveTo(const Vec2 &) override {}

  void OnLineTo(const Vec2 &p1, const Vec2 &p2) override;

  void OnQuadTo(const Vec2 &, const Vec2 &, const Vec2 &) override {}

  void OnConicTo(const Vec2 &p1, const Vec2 &p2, const Vec2 &p3,
                 float weight) override {}

  void OnCubicTo(const Vec2 &p1, const Vec2 &p2, const Vec2 &p3,
                 const Vec2 &p4) override {}

 private:
  float x_;
  float y_;
  int32_t winding_ = 0;
  int32_t on_curve_count_ = 0;
  int32_t ray_intesects_vertex_count_ = 0;
};

}  // namespace skity

#endif  // SRC_GRAPHIC_PATH_SCANNER_HPP
