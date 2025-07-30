// Copyright 2020 Google Inc.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GEOMETRY_WANGS_FORMULA_HPP
#define SRC_GEOMETRY_WANGS_FORMULA_HPP

#include <algorithm>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/point.hpp>

// Wang's formula gives the minimum number of evenly spaced (in the parametric
// sense) line segments that a bezier curve must be chopped into in order to
// guarantee all lines stay within a distance of "1/precision" pixels from the
// true curve. Its definition for a bezier curve of degree "n" is as follows:
//
//     maxLength = max([length(p[i+2] - 2p[i+1] + p[i]) for (0 <= i <= n-2)])
//     numParametricSegments = sqrt(maxLength * precision * n*(n - 1)/8)
//
// (Goldman, Ron. (2003). 5.6.3 Wang's Formula. "Pyramid Algorithms: A Dynamic
// Programming Approach to Curves and Surfaces for Geometric Modeling". Morgan
// Kaufmann Publishers.)
namespace skity::wangs_formula {

// Returns the value by which to multiply length in Wang's formula. (See above.)
template <int Degree>
constexpr float LengthTerm(float precision) {
  return (Degree * (Degree - 1) / 8.f) * precision;
}
template <int Degree>
constexpr float LengthTermP2(float precision) {
  return ((Degree * Degree) * ((Degree - 1) * (Degree - 1)) / 64.f) *
         (precision * precision);
}

union SkFloatIntUnion {
  float fFloat;
  int32_t fSignBitInt;
};

// Helper to see a float as its bit pattern (w/o aliasing warnings)
static inline int32_t Float2Bits(float x) {
  SkFloatIntUnion data;
  data.fFloat = x;
  return data.fSignBitInt;
}

// Returns the log2 of the provided value, were that value to be rounded up to
// the next power of 2. Returns 0 if value <= 0: Never returns a negative
// number, even if value is NaN.
//
//     FloatNextLog2((-inf..1]) -> 0
//     FloatNextLog2((1..2]) -> 1
//     FloatNextLog2((2..4]) -> 2
//     FloatNextLog2((4..8]) -> 3
//     ...
static inline int FloatNextLog2(float x) {
  uint32_t bits = (uint32_t)Float2Bits(x);
  bits += (1u << 23) - 1u;  // Increment the exponent for non-powers-of-2.
  int exp = ((int32_t)bits >> 23) - 127;
  return exp & ~(exp >> 31);  // Return 0 for negative or denormalized floats,
                              // and exponents < 0.
}

inline float Root4(float x) { return sqrtf(sqrtf(x)); }

// Returns nextlog2(sqrt(x)):
//
//   log2(sqrt(x)) == log2(x^(1/2)) == log2(x)/2 == log2(x)/log2(4) == log4(x)
//
inline int Nextlog4(float x) { return (FloatNextLog2(x) + 1) >> 1; }

// Returns nextlog2(sqrt(sqrt(x))):
//
//   log2(sqrt(sqrt(x))) == log2(x^(1/4)) == log2(x)/4 == log2(x)/log2(16) ==
//   log16(x)
//
inline int Nextlog16(float x) { return (FloatNextLog2(x) + 3) >> 2; }

// Represents the upper-left 2x2 matrix of an affine transform for applying
// to
// vectors:
//
//     VectorXform(p1 - p0) == M * float3(p1, 1) - M * float3(p0, 1)
//
class VectorXform {
 public:
  VectorXform() : fC0{1.0f, 0.f}, fC1{0.f, 1.f} {}
  explicit VectorXform(const Matrix& m) { *this = m; }

  VectorXform& operator=(const Matrix& m) {
    fC0 = {m.GetScaleX(), m.GetSkewX()};
    fC1 = {m.GetSkewY(), m.GetScaleY()};
    return *this;
  }

  Vec2 operator()(Vec2 vector) const { return fC0 * vector.x + fC1 * vector.y; }
  Vec4 operator()(Vec4 vectors) const {
    auto xy = fC0 * vectors.x + fC1 * vectors.y;
    auto zw = fC0 * vectors.z + fC1 * vectors.w;
    return Vec4(xy.x, xy.y, zw.x, zw.y);
  }

 private:
  // First and second columns of 2x2 matrix
  Vec2 fC0;
  Vec2 fC1;
};

// Returns Wang's formula, raised to the 4th power, specialized for a quadratic
// curve.
inline float QuadraticP4(float precision, Vec2 p0, Vec2 p1, Vec2 p2,
                         const VectorXform& vectorXform = VectorXform()) {
  Vec2 v = -2.0f * p1 + p0 + p2;
  v = vectorXform(v);
  Vec2 vv = v * v;
  return (vv[0] + vv[1]) * LengthTermP2<2>(precision);
}
inline float QuadraticP4(float precision, const Vec2 pts[],
                         const VectorXform& vectorXform = VectorXform()) {
  return QuadraticP4(precision, pts[0], pts[1], pts[2], vectorXform);
}

// Returns Wang's formula specialized for a quadratic curve.
inline float Quadratic(float precision, const Vec2 pts[],
                       const VectorXform& vectorXform = VectorXform()) {
  return Root4(QuadraticP4(precision, pts, vectorXform));
}

// Returns the log2 value of Wang's formula specialized for a quadratic curve,
// rounded up to the next int.
inline int QuadraticLog2(float precision, const Vec2 pts[],
                         const VectorXform& vectorXform = VectorXform()) {
  // Nextlog16(x) == ceil(log2(sqrt(sqrt(x))))
  return Nextlog16(QuadraticP4(precision, pts, vectorXform));
}

// Returns Wang's formula, raised to the 4th power, specialized for a cubic
// curve.
inline float CubicP4(float precision, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
                     const VectorXform& vectorXform = VectorXform()) {
  Vec4 p01{p0.x, p0.y, p1.x, p1.y};
  Vec4 p12{p1.x, p1.y, p2.x, p2.y};
  Vec4 p23{p2.x, p2.y, p3.x, p3.y};
  Vec4 v = -2.0f * p12 + p01 + p23;
  v = vectorXform(v);
  Vec4 vv = v * v;
  return std::max(vv[0] + vv[1], vv[2] + vv[3]) * LengthTermP2<3>(precision);
}
inline float CubicP4(float precision, const Vec2 pts[],
                     const VectorXform& vectorXform = VectorXform()) {
  return CubicP4(precision, pts[0], pts[1], pts[2], pts[3], vectorXform);
}

// Returns Wang's formula specialized for a cubic curve.
inline float Cubic(float precision, const Vec2 pts[],
                   const VectorXform& vectorXform = VectorXform()) {
  return Root4(CubicP4(precision, pts, vectorXform));
}

// Returns the log2 value of Wang's formula specialized for a cubic curve,
// rounded up to the next int.
inline int CubicLog2(float precision, const Vec2 pts[],
                     const VectorXform& vectorXform = VectorXform()) {
  // Nextlog16(x) == ceil(log2(sqrt(sqrt(x))))
  return Nextlog16(CubicP4(precision, pts, vectorXform));
}

// Returns the maximum number of line segments a cubic with the given
// device-space bounding box size would ever need to be divided into, raised to
// the 4th power. This is simply a special case of the cubic formula where we
// maximize its value by placing control points on specific corners of the
// bounding box.
inline float WorstCaseCubicP4(float precision, float devWidth,
                              float devHeight) {
  float kk = LengthTermP2<3>(precision);
  return 4 * kk * (devWidth * devWidth + devHeight * devHeight);
}

// Returns the maximum number of line segments a cubic with the given
// device-space bounding box size would ever need to be divided into.
inline float WorstCaseCubic(float precision, float devWidth, float devHeight) {
  return Root4(WorstCaseCubicP4(precision, devWidth, devHeight));
}

// Returns the maximum log2 number of line segments a cubic with the given
// device-space bounding box size would ever need to be divided into.
inline int WorstCaseCubicLog2(float precision, float devWidth,
                              float devHeight) {
  // Nextlog16(x) == ceil(log2(sqrt(sqrt(x))))
  return Nextlog16(WorstCaseCubicP4(precision, devWidth, devHeight));
}

// Returns Wang's formula specialized for a conic curve, raised to the second
// power. Input points should be in projected space.
//
// This is not actually due to Wang, but is an analogue from (Theorem 3,
// corollary 1):
//   J. Zheng, T. Sederberg. "Estimating Tessellation Parameter Intervals for
//   Rational Curves and Surfaces." ACM Transactions on Graphics 19(1). 2000.
inline float ConicP2(float precision, Vec2 p0, Vec2 p1, Vec2 p2, float w,
                     const VectorXform& vectorXform = VectorXform()) {
  p0 = vectorXform(p0);
  p1 = vectorXform(p1);
  p2 = vectorXform(p2);

  // Compute center of bounding box in projected space
  const Vec2 C = 0.5f * (Vec2::Min(Vec2::Min(p0, p1), p2) +
                         Vec2::Max(Vec2::Max(p0, p1), p2));

  // Translate by -C. This improves translation-invariance of the formula,
  // see Sec. 3.3 of cited paper
  p0 -= C;
  p1 -= C;
  p2 -= C;

  // Compute max length
  const float max_len = sqrtf(std::max(
      Vec2::Dot(p0, p0), std::max(Vec2::Dot(p1, p1), Vec2::Dot(p2, p2))));

  // Compute forward differences
  const Vec2 dp = -2 * w * p1 + p0 + p2;
  const float dw = fabsf(-2 * w + 2);

  // Compute numerator and denominator for parametric step size of
  // linearization. Here, the epsilon referenced from the cited paper is
  // 1/precision.
  const float rp_minus_1 = std::max(0.f, max_len * precision - 1);
  const float numer =
      std::sqrt(Vec2::Dot(dp, dp)) * precision + rp_minus_1 * dw;
  const float denom = 4 * std::min(w, 1.f);

  // Number of segments = sqrt(numer / denom).
  // This assumes parametric interval of curve being linearized is [t0,t1] = [0,
  // 1]. If not, the number of segments is (tmax - tmin) / sqrt(denom / numer).
  return numer / denom;
}
inline float ConicP2(float precision, const Vec2 pts[], float w,
                     const VectorXform& vectorXform = VectorXform()) {
  return ConicP2(precision, Vec2(pts[0]), Vec2(pts[1]), Vec2(pts[2]), w,
                 vectorXform);
}

// Returns the value of Wang's formula specialized for a conic curve.
inline float Conic(float tolerance, const Vec2 pts[], float w,
                   const VectorXform& vectorXform = VectorXform()) {
  return sqrtf(ConicP2(tolerance, pts, w, vectorXform));
}

// Returns the log2 value of Wang's formula specialized for a conic curve,
// rounded up to the next int.
inline int ConicLog2(float tolerance, const Vec2 pts[], float w,
                     const VectorXform& vectorXform = VectorXform()) {
  // Nextlog4(x) == ceil(log2(sqrt(x)))
  return Nextlog4(ConicP2(tolerance, pts, w, vectorXform));
}

}  // namespace skity::wangs_formula

#endif  // SRC_GEOMETRY_WANGS_FORMULA_HPP
