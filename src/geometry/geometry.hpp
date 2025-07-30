// Copyright 2006 The Android Open Source Project

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GEOMETRY_GEOMETRY_HPP
#define SRC_GEOMETRY_GEOMETRY_HPP

#include <array>
#include <skity/geometry/point.hpp>
#include <vector>

#include "src/geometry/math.hpp"

namespace skity {

enum {
  GEOMETRY_CURVE_RASTER_LIMIT = 128,
};

enum class RotationDirection {
  kCW,
  kCCW,
};

static inline Vec2 FromPoint(Point const& p) { return Vec2{p.x, p.y}; }

static inline Point ToPoint(Vec2 const& x) { return Point{x.x, x.y, 0, 1}; }

/**
 * use for : eval(t) = A * t ^ 2 + B * t + C
 */
struct QuadCoeff {
  Vec2 A{};
  Vec2 B{};
  Vec2 C{};

  QuadCoeff() = default;
  QuadCoeff(Vec2 const& a, Vec2 const& b, Vec2 const& c) : A{a}, B{b}, C{c} {}

  explicit QuadCoeff(std::array<Point, 3> const& src);

  explicit QuadCoeff(std::array<Vec2, 3> const& src);

  Point EvalAt(float t);

  Vec2 eval(float t);

  Vec2 eval(Vec2 const& tt);

  static Point EvalQuadAt(std::array<Point, 3> const& src, float t);

  static void EvalQuadAt(std::array<Point, 3> const& src, float t, Point* outP,
                         Vector* outTangent);

  static Vector EvalQuadTangentAt(std::array<Point, 3> const& src, float t);

  static Vec2 EvalQuadTangentAt(Vec2 const& p1, Vec2 const& p2, Vec2 const& p3,
                                float t);

  static void ChopQuadAt(const Point src[3], Point dst[5], float t);
};

/**
 * use for : eval(t) = A * t ^ 3 + B * t ^ 2 + C * t + D
 */
struct CubicCoeff {
  Vec2 A{};
  Vec2 B{};
  Vec2 C{};
  Vec2 D{};

  explicit CubicCoeff(std::array<Point, 4> const& src);
  explicit CubicCoeff(std::array<Vec2, 4> const& src);

  Point EvalAt(float t);

  Vec2 eval(float t);

  Vec2 eval(Vec2 const& t);

  Vec2 EvalTangentAt(float t);

  static void EvalCubicAt(const Point src[4], float t, Point* loc,
                          Vector* tangent, Vector* curvature);

  static void ChopCubicAt(const Point src[4], Point dst[7], float t);
};

struct Conic;

struct ConicCoeff {
  explicit ConicCoeff(Conic const& conic);

  Vec2 eval(float t);

  QuadCoeff numer;
  QuadCoeff denom;
};

static inline int valid_unit_divide(float number, float denom, float* radio) {
  if (number < 0) {
    number = -number;
    denom = -denom;
  }

  if (denom == 0 || number == 0 || number >= denom) {
    return 0;
  }

  float r = number / denom;
  if (FloatIsNan(r)) {
    return 0;
  }

  if (r == 0) {
    return 0;
  }

  *radio = r;
  return 1;
}

static inline int return_check_zero(int value) {
  if (value == 0) {
    return 0;
  }
  return value;
}

static inline int FindUnitQuadRoots(float A, float B, float C, float roots[2]) {
  if (A == 0) {
    return return_check_zero(valid_unit_divide(-C, B, roots));
  }

  float* r = roots;
  double dr = static_cast<double>(B) * B - 4 * static_cast<double>(A) * C;
  if (dr < 0) {
    return return_check_zero(0);
  }

  dr = glm::sqrt(dr);
  float R = static_cast<float>(dr);
  if (glm::isinf(R)) {
    return return_check_zero(0);
  }

  float Q = (B < 0) ? -(B - R) / 2 : -(B + R) / 2;
  r += valid_unit_divide(Q, A, r);
  r += valid_unit_divide(C, Q, r);
  if (r - roots == 2) {
    if (roots[0] > roots[1]) {
      std::swap(roots[0], roots[1]);
    } else if (roots[0] == roots[1]) {
      r -= 1;
    }
  }

  return return_check_zero(static_cast<int>(r - roots));
}

/**
 * Returns the distance squared from the point to the line segment
 *
 * @param pt          point
 * @param lineStart   start point of the line
 * @param lineEnd     end point of the line
 *
 * @return distance squared value
 */
float pt_to_line(Point const& pt, Point const& lineStart, Point const& lineEnd);

void SubDividedCubic(const Point cubic[4], Point sub_cubic1[4],
                     Point sub_cubic2[4]);

void SubDividedQuad(const Point quad[3], Point sub_quad1[3],
                    Point sub_quad2[3]);

int ChopQuadAtYExtrema(const Point src[3], Point dst[5]);

/**
 * Returns the interpolated unit vectors under constant angular speed.
 *
 * @param start   unit vector
 * @param end     unit vector, and radian between start and end must be less
 * than or equal PI
 * @param num     must greater than 1
 *
 * @return        array of interpolated vectors to return
 */
std::vector<Vec2> CircleInterpolation(Vec2 start, Vec2 end, size_t num);

}  // namespace skity

#endif  // SRC_GEOMETRY_GEOMETRY_HPP
