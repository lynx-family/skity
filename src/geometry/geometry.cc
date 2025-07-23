// Copyright 2006 The Android Open Source Project

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/geometry/geometry.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

#include "src/geometry/conic.hpp"
#include "src/geometry/point_priv.hpp"

namespace skity {

static Vector eval_cubic_derivative(const Point src[4], float t) {
  QuadCoeff coeff;
  Vec2 P0 = FromPoint(src[0]);
  Vec2 P1 = FromPoint(src[1]);
  Vec2 P2 = FromPoint(src[2]);
  Vec2 P3 = FromPoint(src[3]);

  coeff.A = P3 + Vec2{3, 3} * (P1 - P2) - P0;
  coeff.B = Times2(P2 - Times2(P1) + P0);
  coeff.C = P1 - P0;
  Vec2 ret = coeff.eval(t);
  return Vector{ret.x, ret.y, 0, 0};
}

static Vector eval_cubic_2ndDerivative(const Point src[4], float t) {
  Vec2 P0 = FromPoint(src[0]);
  Vec2 P1 = FromPoint(src[1]);
  Vec2 P2 = FromPoint(src[2]);
  Vec2 P3 = FromPoint(src[3]);
  Vec2 A = P3 + Vec2{3, 3} * (P1 - P2) - P0;
  Vec2 B = P2 - Times2(P1) + P0;

  Vec2 vec = A * Vec2{t, t} + B;
  return Vec4{vec.x, vec.y, 0, 0};
}

QuadCoeff::QuadCoeff(std::array<Point, 3> const& src) {
  C = FromPoint(src[0]);
  Vec2 P1 = FromPoint(src[1]);
  Vec2 P2 = FromPoint(src[2]);
  B = Times2(P1 - C);
  A = P2 - Times2(P1) + C;
}

QuadCoeff::QuadCoeff(std::array<Vec2, 3> const& src) {
  C = src[0];
  Vec2 P1 = src[1];
  Vec2 P2 = src[2];
  B = Times2(P1 - C);
  A = P2 - Times2(P1) + C;
}

Point QuadCoeff::EvalAt(float t) { return Point{eval(t), 0, 1}; }

Vec2 QuadCoeff::eval(float t) { return eval(Vec2{t, t}); }

Vec2 QuadCoeff::eval(Vec2 const& tt) { return (A * tt + B) * tt + C; }

Point QuadCoeff::EvalQuadAt(std::array<Point, 3> const& src, float t) {
  return ToPoint(QuadCoeff{src}.eval(t));
}

void QuadCoeff::EvalQuadAt(std::array<Point, 3> const& src, float t,
                           Point* outP, Vector* outTangent) {
  if (t < 0) {
    t = 0;
  }
  if (t > Float1) {
    t = Float1;
  }

  if (outP) {
    *outP = EvalQuadAt(src, t);
  }

  if (outTangent) {
    *outTangent = EvalQuadTangentAt(src, t);
  }
}

Vector QuadCoeff::EvalQuadTangentAt(std::array<Point, 3> const& src, float t) {
  if ((t == 0 && src[0] == src[1]) || (t == 1 && src[1] == src[2])) {
    return src[2] - src[0];
  }

  Vec2 P0 = FromPoint(src[0]);
  Vec2 P1 = FromPoint(src[1]);
  Vec2 P2 = FromPoint(src[2]);

  Vec2 B = P1 - P0;
  Vec2 A = P2 - P1 - B;
  Vec2 T = A * Vec2{t, t} + B;

  return Vector{T + T, 0, 0};
}

Vec2 QuadCoeff::EvalQuadTangentAt(Vec2 const& p1, Vec2 const& p2,
                                  Vec2 const& p3, float t) {
  Vec2 B = p2 - p1;
  Vec2 A = p3 - p2 - B;
  Vec2 T = A * Vec2{t, t} + B;

  return glm::normalize(T);
}

void QuadCoeff::ChopQuadAt(const Point src[3], Point dst[5], float t) {
  assert(t > 0 && t < Float1);

  Vec2 p0 = FromPoint(src[0]);
  Vec2 p1 = FromPoint(src[1]);
  Vec2 p2 = FromPoint(src[2]);
  Vec2 tt{t};

  Vec2 p01 = Interp(p0, p1, tt);
  Vec2 p12 = Interp(p1, p2, tt);

  dst[0] = ToPoint(p0);
  dst[1] = ToPoint(p01);
  dst[2] = ToPoint(Interp(p01, p12, tt));
  dst[3] = ToPoint(p12);
  dst[4] = ToPoint(p2);
}

CubicCoeff::CubicCoeff(std::array<Point, 4> const& src) {
  Vec2 P0 = FromPoint(src[0]);
  Vec2 P1 = FromPoint(src[1]);
  Vec2 P2 = FromPoint(src[2]);
  Vec2 P3 = FromPoint(src[3]);
  Vec2 three{3, 3};

  A = P3 + three * (P1 - P2) - P0;
  B = three * (P2 - Times2(P1) + P0);
  C = three * (P1 - P0);
  D = P0;
}

CubicCoeff::CubicCoeff(std::array<Vec2, 4> const& src) {
  auto p0 = src[0];
  auto p1 = src[1];
  auto p2 = src[2];
  auto p3 = src[3];

  Vec2 three{3, 3};

  A = p3 + three * (p1 - p2) - p0;
  B = three * (p2 - Times2(p1) + p0);
  C = three * (p1 - p0);
  D = p0;
}

Point CubicCoeff::EvalAt(float t) { return Point{eval(t), 0, 1}; }

Vec2 CubicCoeff::eval(float t) { return eval(Vec2{t, t}); }

Vec2 CubicCoeff::eval(Vec2 const& t) { return ((A * t + B) * t + C) * t + D; }

Vec2 CubicCoeff::EvalTangentAt(float t) {
  return 3.f * A * t * t + 2.f * B * t + C;
}

void CubicCoeff::EvalCubicAt(const Point src[4], float t, Point* loc,
                             Vector* tangent, Vector* curvature) {
  if (loc) {
    *loc =
        ToPoint(CubicCoeff(std::array<Point, 4>{src[0], src[1], src[2], src[3]})
                    .eval(t));
  }

  if (tangent) {
    // The derivative equation returns a zero tangent vector when t is 0 or 1,
    // and the adjacent control point is equal to the end point. In this case,
    // use the next control point or the end points to compute the tangent.
    if ((t == 0 && src[0] == src[1]) || (t == 1 && src[2] == src[3])) {
      if (t == 0) {
        *tangent = src[2] - src[0];
      } else {
        *tangent = src[3] - src[1];
      }

      if (!tangent->x && !tangent->y) {
        *tangent = src[3] - src[0];
      }
    } else {
      *tangent = eval_cubic_derivative(src, t);
    }
  }

  if (curvature) {
    *curvature = eval_cubic_2ndDerivative(src, t);
  }
}

void CubicCoeff::ChopCubicAt(const Point src[4], Point dst[7], float t) {
  Vec2 p0 = FromPoint(src[0]);
  Vec2 p1 = FromPoint(src[1]);
  Vec2 p2 = FromPoint(src[2]);
  Vec2 p3 = FromPoint(src[3]);
  Vec2 tt{t, t};

  Vec2 ab = Interp(p0, p1, tt);
  Vec2 bc = Interp(p1, p2, tt);
  Vec2 cd = Interp(p2, p3, tt);
  Vec2 abc = Interp(ab, bc, tt);
  Vec2 bcd = Interp(bc, cd, tt);
  Vec2 abcd = Interp(abc, bcd, tt);

  dst[0] = ToPoint(p0);
  dst[1] = ToPoint(ab);
  dst[2] = ToPoint(abc);
  dst[3] = ToPoint(abcd);
  dst[4] = ToPoint(bcd);
  dst[5] = ToPoint(cd);
  dst[6] = ToPoint(p3);
}

ConicCoeff::ConicCoeff(Conic const& conic) {
  Vec2 P0 = FromPoint(conic.pts[0]);
  Vec2 P1 = FromPoint(conic.pts[1]);
  Vec2 P2 = FromPoint(conic.pts[2]);
  Vec2 ww{conic.w, conic.w};

  Vec2 p1w = P1 * ww;
  numer.C = P0;
  numer.A = P2 - Times2(p1w) + P0;
  numer.B = Times2(p1w - P0);

  denom.C = Vec2{1, 1};
  denom.B = Times2(ww - denom.C);
  denom.A = Vec2{0, 0} - denom.B;
}

Vec2 ConicCoeff::eval(float t) {
  Vec2 tt{t, t};
  Vec2 n = numer.eval(tt);
  Vec2 d = denom.eval(tt);
  return n / d;
}

float pt_to_line(Point const& pt, Point const& lineStart,
                 Point const& lineEnd) {
  Vector dxy = lineEnd - lineStart;
  Vector ab0 = pt - lineStart;

  float number = VectorDotProduct(dxy, ab0);
  float denom = VectorDotProduct(dxy, dxy);
  float t = SkityIEEEFloatDivided(number, denom);
  if (t >= 0 && t <= 1) {
    Point hit;
    hit.x = lineStart.x * (1 - t) + lineEnd.x * t;
    hit.y = lineStart.y * (1 - t) + lineEnd.y * t;
    hit.z = 0;
    hit.w = 1;
    return PointDistanceToSqd(hit, pt);
  } else {
    return PointDistanceToSqd(pt, lineStart);
  }
}

void SubDividedCubic(const Point cubic[4], Point sub_cubic1[4],
                     Point sub_cubic2[4]) {
  Point p1 = (cubic[0] + cubic[1]) / 2.f;
  Point p2 = (cubic[1] + cubic[2]) / 2.f;
  Point p3 = (cubic[2] + cubic[3]) / 2.f;
  Point p4 = (p1 + p2) / 2.f;
  Point p5 = (p2 + p3) / 2.f;
  Point p6 = (p4 + p5) / 2.f;

  Point p0 = cubic[0];
  Point p7 = cubic[3];

  sub_cubic1[0] = p0;
  sub_cubic1[1] = p1;
  sub_cubic1[2] = p4;
  sub_cubic1[3] = p6;

  sub_cubic2[0] = p6;
  sub_cubic2[1] = p5;
  sub_cubic2[2] = p3;
  sub_cubic2[3] = p7;
}

void SubDividedQuad(const Point quad[3], Point sub_quad1[3],
                    Point sub_quad2[3]) {
  Point p1 = (quad[0] + quad[1]) * 0.5f;
  Point p2 = (quad[1] + quad[2]) * 0.5f;
  Point p3 = (p1 + p2) * 0.5f;

  sub_quad1[0] = quad[0];
  sub_quad1[1] = p1;
  sub_quad1[2] = p3;

  sub_quad2[0] = p3;
  sub_quad2[1] = p2;
  sub_quad2[2] = quad[2];
}

int is_not_monotonic(float a, float b, float c) {
  float ab = a - b;
  float bc = b - c;
  if (ab < 0) {
    bc = -bc;
  }
  return ab == 0 || bc < 0;
}

/*  Returns 0 for 1 quad, and 1 for two quads, either way the answer is
 stored in dst[]. Guarantees that the 1/2 quads will be monotonic.
 */
int ChopQuadAtYExtrema(const Point src[3], Point dst[5]) {
  float a = src[0].y;
  float b = src[1].y;
  float c = src[2].y;

  if (is_not_monotonic(a, b, c)) {
    float tValue;
    if (valid_unit_divide(a - b, a - b - b + c, &tValue)) {
      QuadCoeff::ChopQuadAt(src, dst, tValue);
      // This is to solve the problem of numerical accuracy. These values
      // ​​should be the same, because the tangent of the extreme point of y
      // must be parallel to the x-axis.
      dst[1].y = dst[3].y = dst[2].y;
      return 1;
    }
    // if we get here, we need to force dst to be monotonic, even though
    // we couldn't compute a unit_divide value (probably underflow).
    b = std::fabs(a - b) < std::fabs(b - c) ? a : c;
  }
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];

  dst[1].y = b;

  return 0;
}

std::vector<Vec2> CircleInterpolation(Vec2 start, Vec2 end, size_t num) {
  std::vector<Vec2> result(num);
  const auto cos_theta = glm::dot(start, end);
  num = std::max(num, static_cast<size_t>(1));
  float step = 1.f / num;
  if (std::fabs(cos_theta) < 0.99) {
    // slerp
    const auto theta = std::acos(cos_theta);
    const auto sin_theta = std::sin(theta);
    for (size_t i = 1; i < num + 1; i++) {
      float t = step * i;
      const auto complement_tt = std::sin((1.f - t) * theta) / sin_theta;
      const auto tt = std::sin(t * theta) / sin_theta;
      result[i - 1] = complement_tt * start + tt * end;
    }
  } else {
    if (cos_theta > 0) {
      // lerp
      for (size_t i = 1; i < num + 1; i++) {
        float t = step * i;
        const auto complement_t = 1.f - t;
        result[i - 1] = glm::normalize((complement_t * start + t * end));
      }
    } else {
      // rotate
      bool CW = CrossProduct(start, end) < 0;
      const auto theta = std::acos(cos_theta);
      const auto rotate_theta = theta / num;
      const auto cos_rotate_theta = std::cos(rotate_theta);
      const auto sin_rotate_theta = std::sin(rotate_theta);
      Vec2 pending_rotate_vec = start;
      for (size_t i = 1; i < num; i++) {
        Vec2 rotate_vec;
        if (CW) {
          rotate_vec.x = cos_rotate_theta * pending_rotate_vec.x +
                         sin_rotate_theta * pending_rotate_vec.y;
          rotate_vec.y = cos_rotate_theta * pending_rotate_vec.y -
                         sin_rotate_theta * pending_rotate_vec.x;
        } else {
          rotate_vec.x = cos_rotate_theta * pending_rotate_vec.x -
                         sin_rotate_theta * pending_rotate_vec.y;
          rotate_vec.y = sin_rotate_theta * pending_rotate_vec.x +
                         cos_rotate_theta * pending_rotate_vec.y;
        }

        pending_rotate_vec = rotate_vec;
        result[i - 1] = rotate_vec;
      }
      result[num - 1] = end;
    }
  }
  return result;
}

}  // namespace skity
