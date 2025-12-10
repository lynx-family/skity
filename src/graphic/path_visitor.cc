// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/path_visitor.hpp"

#include "src/geometry/conic.hpp"
#include "src/geometry/geometry.hpp"
#include "src/geometry/wangs_formula.hpp"
#include "src/logging.hpp"

namespace skity {

constexpr static float kPrecision = 4.0f;
constexpr static float kMaxPrecision = 10000.f;

namespace {

float get_persp_ratio(const Matrix& transform) {
  std::array<Vec2, 3> src{Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1}};
  std::array<Vec2, 3> dst;
  transform.MapPoints(dst.data(), src.data(), 3);
  float d_x = (dst[1] - dst[0]).Length();
  float d_y = (dst[2] - dst[0]).Length();
  return std::sqrt(d_x * d_y);
}

// only used with persp matrix for now
float device_precision_to_local_precision(float device_precision,
                                          const Path& path,
                                          const Matrix& transform) {
  const Rect bounds = path.GetBounds();
  std::array<Vec2, 4> points{Vec2{bounds.Left(), bounds.Top()},
                             Vec2{bounds.Right(), bounds.Top()},
                             Vec2{bounds.Left(), bounds.Bottom()},
                             Vec2{bounds.Right(), bounds.Bottom()}};
  float ratio = 0.f;
  for (auto& point : points) {
    Matrix scaled_transform = transform * Matrix::Translate(point.x, point.y);
    float persp_ratio = get_persp_ratio(scaled_transform);
    ratio = std::max(ratio, persp_ratio);
  }

  return std::min(kMaxPrecision, device_precision * ratio);
}

}  // namespace

void PathVisitor::VisitPath(const Path& path, bool force_close) {
  Path::Iter iter{path, force_close};
  std::array<Point, 4> pts = {};

  this->OnBeginPath();

  float precision = kPrecision;
  if (matrix_.HasPersp()) {
    precision = device_precision_to_local_precision(kPrecision, path, matrix_);
  }

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
                     reinterpret_cast<const Vec2&>(pts[2]), precision);
        break;
      case Path::Verb::kConic:
        HandleConicTo(reinterpret_cast<const Vec2&>(pts[0]),
                      reinterpret_cast<const Vec2&>(pts[1]),
                      reinterpret_cast<const Vec2&>(pts[2]), iter.ConicWeight(),
                      precision);
        break;
      case Path::Verb::kCubic:
        HandleCubicTo(reinterpret_cast<const Vec2&>(pts[0]),
                      reinterpret_cast<const Vec2&>(pts[1]),
                      reinterpret_cast<const Vec2&>(pts[2]),
                      reinterpret_cast<const Vec2&>(pts[3]), precision);
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

void PathVisitor::HandleQuadTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                               float precision) {
  if (!approx_curve_) {
    OnQuadTo(p1, p2, p3);
    prev_pt_ = p3;
    return;
  }

  std::array<Vec2, 3> arc;
  arc[0] = p1;
  arc[1] = p2;
  arc[2] = p3;

  float num = 0.f;
  if (matrix_.HasPersp()) {
    num = std::ceil(wangs_formula::Quadratic(precision, arc.data()));
  } else {
    num = std::ceil(wangs_formula::Quadratic(
        kPrecision, arc.data(), wangs_formula::VectorXform(matrix_)));
  }
  if (num <= 1.0f) {
    HandleLineTo(p1, p3);
    return;
  }
  DEBUG_CHECK(num < (1 << 10));

  QuadCoeff coeff(arc);
  Vec2 prev_point = p1;
  for (int i = 1; i <= num; i++) {
    float t = i / num;
    Vec2 curr_point = coeff.eval(t);
    HandleLineTo(prev_point, curr_point);
    prev_point = curr_point;
  }
}

void PathVisitor::HandleConicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                                float weight, float precision) {
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

  HandleQuadTo(Vec2{quads[0]}, Vec2{quads[1]}, Vec2{quads[2]}, precision);
  HandleQuadTo(Vec2{quads[2]}, Vec2{quads[3]}, Vec2{quads[4]}, precision);
}

void PathVisitor::HandleCubicTo(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                                Vec2 const& p4, float precision) {
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
  float num = 0.f;
  if (matrix_.HasPersp()) {
    num = std::ceil(wangs_formula::Cubic(precision, arc.data()));
  } else {
    num = std::ceil(wangs_formula::Cubic(kPrecision, arc.data(),
                                         wangs_formula::VectorXform(matrix_)));
  }
  if (num <= 1.0f) {
    HandleLineTo(p1, p4);
    return;
  }
  DEBUG_CHECK(num < (1 << 10));

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
