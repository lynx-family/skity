// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GEOMETRY_CUBIC_HPP
#define SRC_GEOMETRY_CUBIC_HPP

#include <array>
#include <memory>
#include <skity/geometry/vector.hpp>
#include <vector>

#include "src/geometry/geometry.hpp"

namespace skity {

class Cubic {
 public:
  Cubic(const Vec2& p1, const Vec2& c1, const Vec2& c2, const Vec2& p2);

  std::vector<std::array<Vec2, 3>> ToQuads();

 private:
  std::array<Vec2, 4> Subsegment(float t1, float t2);

  Vec2 p1_{};
  Vec2 c1_{};
  Vec2 c2_{};
  Vec2 p2_{};
  std::unique_ptr<CubicCoeff> cc_;
  std::unique_ptr<QuadCoeff> qc_;
};

}  // namespace skity

#endif  // SRC_GEOMETRY_CUBIC_HPP
