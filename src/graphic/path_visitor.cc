// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/path_visitor.hpp"

#include "src/geometry/conic.hpp"
#include "src/geometry/wangs_formula.hpp"

namespace skity {

constexpr static float kPrecision = 4.0f;

static void split_quad(Vec2* base) {
  Vec2 a, b;

  base[4] = base[2];
  a = base[0] + base[1];
  b = base[1] + base[2];
  base[3] = b / 2.f;
  base[2] = (a + b) / 4.f;
  base[1] = a / 2.f;
}

void PathVisitor::VisitPath(const Path& path, bool force_close) {
  Path::Iter iter{path, force_close};
  std::array<Point, 4> pts = {};

  this->OnBeginPath();

  for (;;) {
    Path::Verb verb = iter.Next(pts.data());
    switch (verb) {
      case Path::Verb::kMove:
        HandleMoveTo(reinterpret_cast<const Vec2&>(pts[0]));
        break;
      case Path::Verb::kLine:
        HandleLineTo(reinterpret_cast<const Vec2&>(pts[0]),
                     reinterpret_cast<const Vec2&>(pts[1]));
        break;
      case Path::Verb::kQuad:
        HandleQuadTo(reinterpret_cast<const Vec2&>(pts[0]),
                     reinterpret_cast<const Vec2&>(pts[1]),
                     reinterpret_cast<const Vec2&>(pts[2]));
        break;
      case Path::Verb::kConic:
        HandleConicTo(reinterpret_cast<const Vec2&>(pts[0]),
                      reinterpret_cast<const Vec2&>(pts[1]),
                      reinterpret_cast<const Vec2&>(pts[2]),
                      iter.ConicWeight());
        break;
      case Path::Verb::kCubic:
        HandleCubicTo(reinterpret_cast<const Vec2&>(pts[0]),
                      reinterpret_cast<const Vec2&>(pts[1]),
                      reinterpret_cast<const Vec2&>(pts[2]),
                      reinterpret_cast<const Vec2&>(pts[3]));
        break;
      case Path::Verb::kClose:
        HandleClose();
        break;
      case Path::Verb::kDone:
        goto DONE;
        break;
    }
  }

DONE:
  OnEndPath();
}

void PathVisitor::HandleMoveTo(const Vec2& p) {
  prev_pt_ = p;
  this->OnMoveTo(p);
}

void PathVisitor::HandleLineTo(const Vec2& p1, const Vec2& p2) {
  this->OnLineTo(p1, p2);

  // update prev_dir_ and pt
  prev_pt_ = p2;
}

void PathVisitor::HandleQuadTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3) {
  if (!approx_curve_) {
    OnQuadTo(p1, p2, p3);
    prev_pt_ = p3;
    return;
  }

  std::array<Vec2, 33 * 3 + 1> bez_stack{};
  std::array<int32_t, 33> level_stack{};

  int32_t top, level;
  int32_t* levels = level_stack.data();

  auto arc = bez_stack.data();
  arc[0] = p3;
  arc[1] = p2;
  arc[2] = p1;

  top = 0;

  level = wangs_formula::QuadraticLog2(kPrecision, arc,
                                       wangs_formula::VectorXform(matrix_));

  levels[0] = std::max(0, level);
  do {
    level = levels[top];
    if (level > 0) {
      split_quad(arc);
      arc += 2;
      top++;
      levels[top] = levels[top - 1] = level - 1;
      continue;
    }
    HandleLineTo(prev_pt_, arc[0]);
    top--;
    arc -= 2;
  } while (top >= 0);
}

void PathVisitor::HandleConicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                                float weight) {
  if (!approx_curve_) {
    OnConicTo(p1, p2, p3, weight);
    prev_pt_ = p3;
    return;
  }

  Point start = {p1.x, p1.y, 0.f, 1.f};
  Point control = {p2.x, p2.y, 0.f, 1.f};
  Point end = {p3.x, p3.y, 0.f, 1.f};

  std::array<Point, 5> quads{};
  Conic conic{start, control, end, weight};
  conic.ChopIntoQuadsPOW2(quads.data(), 1);
  quads[0] = start;

  HandleQuadTo(Vec2{quads[0]}, Vec2{quads[1]}, Vec2{quads[2]});
  HandleQuadTo(Vec2{quads[2]}, Vec2{quads[3]}, Vec2{quads[4]});
}

void PathVisitor::HandleCubicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                                Vec2 const& p4) {
  if (!approx_curve_ && p1 != p2 && p3 != p4) {
    OnCubicTo(p1, p2, p3, p4);
    prev_pt_ = p4;
    return;
  }

  std::array<Vec2, 4> arc;
  arc[0] = p1;
  arc[1] = p2;
  arc[2] = p3;
  arc[3] = p4;

  float num = std::ceil(wangs_formula::Cubic(
      kPrecision, arc.data(), wangs_formula::VectorXform(matrix_)));
  if (num <= 1.0f) {
    HandleLineTo(p1, p4);
    return;
  }

  CubicCoeff coeff(arc);
  Vec2 prev_point = p1;
  for (int i = 1; i <= num; i++) {
    float t = i / num;
    Vec2 curr_point = coeff.eval(t);
    HandleLineTo(prev_point, curr_point);
    prev_point = curr_point;
  }
}

void PathVisitor::HandleClose() { OnClose(); }

}  // namespace skity
