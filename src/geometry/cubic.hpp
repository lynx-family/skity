// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GEOMETRY_CUBIC_HPP
#define SRC_GEOMETRY_CUBIC_HPP

#include <array>
#include <memory>
#include <skity/geometry/point.hpp>
#include <vector>

#include "src/geometry/geometry.hpp"

namespace skity {

class Cubic {
 public:
  Cubic(const Point& p1, const Point& c1, const Point& c2, const Point& p2);

  std::vector<std::array<Point, 3>> ToQuads();

 private:
  std::array<Point, 4> Subsegment(float t1, float t2);

  Point p1_{};
  Point c1_{};
  Point c2_{};
  Point p2_{};
  std::unique_ptr<CubicCoeff> cc_;
  std::unique_ptr<QuadCoeff> qc_;
};

}  // namespace skity

#endif  // SRC_GEOMETRY_CUBIC_HPP
