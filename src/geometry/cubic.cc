// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/geometry/cubic.hpp"

#include "src/geometry/point_priv.hpp"

namespace skity {

namespace {

Point cubic_tangent(const Point& p2, const Point& p1) {
  Point p{};
  PointSet(p, 3.f * (p2.x - p1.x), 3.f * (p2.y - p1.y));
  return p;
}

}  // namespace

Cubic::Cubic(const Point& p1, const Point& c1, const Point& c2, const Point& p2)
    : p1_(p1), c1_(c1), c2_(c2), p2_(p2) {
  cc_ = std::make_unique<CubicCoeff>(std::array<Point, 4>{p1, c1, c2, p2});

  qc_ = std::make_unique<QuadCoeff>(std::array<Point, 3>{
      cubic_tangent(c1, p1), cubic_tangent(c2, c1), cubic_tangent(p2, c2)});
}

std::vector<std::array<Point, 3>> Cubic::ToQuads() {
  // This magic number is the square of 36 / sqrt(3).
  // See: http://caffeineowl.com/graphics/2d/vectorial/cubic2quad01.html
  float accuracy = 0.1f;
  auto max_hypot2 = 432.0 * accuracy * accuracy;
  auto p1x2 = c1_ * 3.f - p1_;
  auto p2x2 = c2_ * 3.f - p2_;
  auto p = p2x2 - p1x2;
  auto err = Vec2{p}.Dot(Vec2{p});
  auto quad_count = std::max(1., ceil(pow(err / max_hypot2, 1. / 6.0)));

  std::vector<std::array<Point, 3>> quads;
  quads.reserve(quad_count);
  for (size_t i = 0; i < quad_count; i++) {
    auto t0 = i / quad_count;
    auto t1 = (i + 1) / quad_count;
    auto seg = Subsegment(t0, t1);
    auto p1x2 = seg[1] * 3.f - seg[0];
    auto p2x2 = seg[2] * 3.f - seg[3];
    quads.emplace_back(
        std::array<Point, 3>{seg[0], ((p1x2 + p2x2) / 4.f), seg[3]});
  }
  return quads;
}

std::array<Point, 4> Cubic::Subsegment(float t1, float t2) {
  Point p1 = cc_->EvalAt(t1);
  Point p2 = cc_->EvalAt(t2);
  auto scale = (t2 - t1) * (1.f / 3.f);
  auto c1 = p1 + qc_->EvalAt(t1) * scale;
  auto c2 = p2 - qc_->EvalAt(t2) * scale;
  return {p1, c1, c2, p2};
}

}  // namespace skity
