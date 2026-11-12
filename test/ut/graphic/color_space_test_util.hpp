// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_UT_GRAPHIC_COLOR_SPACE_TEST_UTIL_HPP
#define TEST_UT_GRAPHIC_COLOR_SPACE_TEST_UTIL_HPP

#include <array>

// Independent double-precision reference math for the color space tests. It
// derives gamut matrices straight from published chromaticities and the
// Bradford transform, so assertions built on it do not depend on the
// implementation under test: a test that feeds the implementation its own
// output passes for any self-consistent convention, including a wrong one.
namespace color_space_test_util {

using Mat3 = std::array<double, 9>;  // row-major
using Vec3 = std::array<double, 3>;

// cHRM layout: white point first, then the red / green / blue primaries.
struct Chromaticities {
  double wx, wy, rx, ry, gx, gy, bx, by;
};

// Published chromaticities (both spaces have a D65 native white point).
constexpr Chromaticities kSRGBChromaticities{0.3127, 0.3290, 0.640, 0.330,
                                             0.300,  0.600,  0.150, 0.060};
constexpr Chromaticities kDisplayP3Chromaticities{0.3127, 0.3290, 0.680, 0.320,
                                                  0.265,  0.690,  0.150, 0.060};

// CIE illuminant D50 (XYZ, Y == 1): the connection space reference white.
constexpr Vec3 kD50{0.96422, 1.0, 0.82521};

inline Vec3 XYZFromXY(double x, double y) {
  return {x / y, 1.0, (1.0 - x - y) / y};
}

inline Mat3 Multiply(const Mat3& a, const Mat3& b) {
  Mat3 out{};
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      out[row * 3 + col] = a[row * 3] * b[col] + a[row * 3 + 1] * b[3 + col] +
                           a[row * 3 + 2] * b[6 + col];
    }
  }
  return out;
}

inline Vec3 Apply(const Mat3& m, const Vec3& v) {
  return {m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
          m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
          m[6] * v[0] + m[7] * v[1] + m[8] * v[2]};
}

inline Mat3 Invert(const Mat3& m) {
  double det = m[0] * (m[4] * m[8] - m[5] * m[7]) -
               m[1] * (m[3] * m[8] - m[5] * m[6]) +
               m[2] * (m[3] * m[7] - m[4] * m[6]);
  double inv_det = 1.0 / det;
  return {(m[4] * m[8] - m[5] * m[7]) * inv_det,
          (m[2] * m[7] - m[1] * m[8]) * inv_det,
          (m[1] * m[5] - m[2] * m[4]) * inv_det,
          (m[5] * m[6] - m[3] * m[8]) * inv_det,
          (m[0] * m[8] - m[2] * m[6]) * inv_det,
          (m[2] * m[3] - m[0] * m[5]) * inv_det,
          (m[3] * m[7] - m[4] * m[6]) * inv_det,
          (m[1] * m[6] - m[0] * m[7]) * inv_det,
          (m[0] * m[4] - m[1] * m[3]) * inv_det};
}

// Bradford chromatic adaptation mapping XYZ relative to src_white onto XYZ
// relative to dst_white.
inline Mat3 BradfordAdaptation(const Vec3& src_white, const Vec3& dst_white) {
  const Mat3 cone{0.8951, 0.2664, -0.1614, -0.7502, 1.7135,
                  0.0367, 0.0389, -0.0685, 1.0296};
  Vec3 src = Apply(cone, src_white);
  Vec3 dst = Apply(cone, dst_white);
  Mat3 scaled = cone;
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      scaled[row * 3 + col] *= dst[row] / src[row];
    }
  }
  return Multiply(Invert(cone), scaled);
}

// RGB -> XYZ relative to the space's own (native) white point.
inline Mat3 NativeGamut(const Chromaticities& c) {
  Vec3 r = XYZFromXY(c.rx, c.ry);
  Vec3 g = XYZFromXY(c.gx, c.gy);
  Vec3 b = XYZFromXY(c.bx, c.by);
  Mat3 primaries{r[0], g[0], b[0], r[1], g[1], b[1], r[2], g[2], b[2]};
  // Scale the columns so RGB (1, 1, 1) lands on the white point.
  Vec3 scale = Apply(Invert(primaries), XYZFromXY(c.wx, c.wy));
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      primaries[row * 3 + col] *= scale[col];
    }
  }
  return primaries;
}

// RGB -> XYZ adapted to the D50 connection space: what a gamut_xyz matrix is
// expected to hold.
inline Mat3 D50Gamut(const Chromaticities& c) {
  return Multiply(BradfordAdaptation(XYZFromXY(c.wx, c.wy), kD50),
                  NativeGamut(c));
}

// Undoes the D50 adaptation of a gamut matrix towards D65 and returns the
// native chromaticities the matrix describes.
inline Chromaticities NativeChromaticities(const float gamut_d50[9]) {
  Mat3 m{};
  for (int i = 0; i < 9; i++) {
    m[i] = gamut_d50[i];
  }
  Vec3 white{m[0] + m[1] + m[2], m[3] + m[4] + m[5], m[6] + m[7] + m[8]};
  Mat3 n = Multiply(BradfordAdaptation(white, XYZFromXY(0.3127, 0.3290)), m);
  auto x_of = [](double x, double y, double z) { return x / (x + y + z); };
  auto y_of = [](double x, double y, double z) { return y / (x + y + z); };
  double wx = n[0] + n[1] + n[2];
  double wy = n[3] + n[4] + n[5];
  double wz = n[6] + n[7] + n[8];
  return {x_of(wx, wy, wz),       y_of(wx, wy, wz),
          x_of(n[0], n[3], n[6]), y_of(n[0], n[3], n[6]),
          x_of(n[1], n[4], n[7]), y_of(n[1], n[4], n[7]),
          x_of(n[2], n[5], n[8]), y_of(n[2], n[5], n[8])};
}

}  // namespace color_space_test_util

#endif  // TEST_UT_GRAPHIC_COLOR_SPACE_TEST_UTIL_HPP
