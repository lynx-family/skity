// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/path_scanner.hpp"

#include "src/geometry/math.hpp"

namespace skity {

static bool between(float a, float b, float c) {
  return (a - b) * (c - b) <= 0;
}

static bool check_on_line(float x, float y, const Vec2& start,
                          const Vec2& end) {
  if (start.y == end.y) {
    return between(start.x, x, end.x) && x != end.x;
  } else {
    return FloatNearlyZero(x - start.x) && FloatNearlyZero(y - start.y);
  }
}

PathScanner::PathScanner(float x, float y)
    : PathVisitor(true, Matrix{}), x_(x), y_(y) {}

void PathScanner::OnLineTo(const Vec2& p1, const Vec2& p2) {
  float x0 = p1.x;
  float y0 = p1.y;
  float x1 = p2.x;
  float y1 = p2.y;

  float dy = y1 - y0;

  int32_t dir = 1;

  if (y0 > y1) {
    std::swap(y0, y1);
    dir = -1;
  }

  if (y_ < y0 || y_ > y1) {
    return;
  }

  if (check_on_line(x_, y_, p1, p2)) {
    on_curve_count_ += 1;
    return;
  }

  float cross = (x1 - x0) * (y_ - p1.y) - dy * (x_ - x0);

  if (cross == 0.f) {
    if (x_ != x1 || y_ != p2.y) {
      on_curve_count_ += 1;
    }

    return;
  }

  auto sign = cross * dir;

  if (sign > 0) {
    return;
  }

  if (y_ == p1.y) {
    ray_intesects_vertex_count_ += 1;
  }

  winding_ += dir;
}

}  // namespace skity
