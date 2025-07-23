// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GEOMETRY_CONIC_HPP
#define SRC_GEOMETRY_CONIC_HPP

#include <cstring>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/point.hpp>

#include "src/geometry/geometry.hpp"

namespace skity {

struct Conic {
  enum {
    kMaxConicsForArc = 5,
    kMaxConicToQuadPOW2 = 5,
  };

  static int BuildUnitArc(Vec2 const& start, Vec2 const& stop,
                          RotationDirection dir, Matrix* matrix,
                          Conic conics[kMaxConicsForArc]);

  Conic() = default;

  Conic(Point const& p0, Point const& p1, Point const& p2, float weight)
      : pts{p0, p1, p2}, w(weight) {}

  Conic(Point const p[3], float weight);

  void Set(Point const p[3], float weight) {
    std::memcpy(pts, p, 3 * sizeof(Point));
    w = weight;
  }

  void Set(Vec2 const p[3], float weight) {
    pts[0] = Point(p[0], 0.f, 1.f);
    pts[1] = Point(p[1], 0.f, 1.f);
    pts[2] = Point(p[2], 0.f, 1.f);
    w = weight;
  }

  void Set(const Point& p0, const Point& p1, const Point& p2, float weight) {
    pts[0] = p0;
    pts[1] = p1;
    pts[2] = p2;
    w = weight;
  }

  void Set(Vec3 const& p0, Vec3 const& p1, Vec3 const& p2, float weight) {
    pts[0] = Point(p0, 1);
    pts[1] = Point(p1, 1);
    pts[2] = Point(p2, 1);
    w = weight;
  }

  void Chop(Conic conics[2]) const;
  bool ChopAt(float t, Conic dst[2]) const;
  void ChopAt(float t1, float t2, Conic* dst) const;

  void EvalAt(float t, Point* pos, Vector* tangent = nullptr) const;
  Point EvalAt(float t) const;
  Vector evalTangentAt(float t) const;
  /**
   * @brief Chop this conic into N quads, stored continguously in pts
   *
   * @param pts   quad storage
   * @param pow2  number of quads N = 1 << pow2
   * @return      number of quad storaged in pts
   */
  uint32_t ChopIntoQuadsPOW2(Point pts[], uint32_t pow2);
  Point pts[3] = {};
  float w = 0.f;
};

}  // namespace skity

#endif  // SRC_GEOMETRY_CONIC_HPP
