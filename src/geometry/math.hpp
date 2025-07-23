// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GEOMETRY_MATH_HPP
#define SRC_GEOMETRY_MATH_HPP

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <limits>
#include <skity/geometry/vector.hpp>

namespace skity {

#ifdef __GNUC__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#define Float1 1.0f
#define FloatHalf 0.5f
#define FloatNaN std::numeric_limits<float>::quiet_NaN()
#define FloatInfinity std::numeric_limits<float>::infinity()
#define NearlyZero (Float1 / (1 << 12))
#define FloatRoot2Over2 0.707106781f
#define FloatSqrt2 1.41421356f

/**
 *  Returns -1 || 0 || 1 depending on the sign of value:
 *  -1 if x < 0
 *   0 if x == 0
 *   1 if x > 0
 */
static inline int FloatSignAsInt(float x) { return x < 0 ? -1 : (x > 0); }

static inline int RoundToInt(float x) {
  return static_cast<int>(std::round(x));
}

static inline bool FloatNearlyZero(float x, float tolerance = NearlyZero) {
  return glm::abs(x) <= tolerance;
}

static inline float FloatInterp(float A, float B, float t) {
  return A + (B - A) * t;
}

static inline float FloatInterpFunc(float search_key, const float keys[],
                                    const float values[], int length) {
  int right = 0;
  while (right < length && keys[right] < search_key) {
    ++right;
  }
  if (right == length) {
    return values[length - 1];
  }
  if (right == 0) {
    return values[0];
  }
  // Otherwise, interpolate between right - 1 and right.
  float range_left = keys[right - 1];
  float range_right = keys[right];
  float t = (search_key - range_left) / (range_right - range_left);
  return FloatInterp(values[right - 1], values[right], t);
}

static inline float SkityFloatHalf(float v) { return v * FloatHalf; }

static inline bool FloatIsNan(float x) { return x != x; }

[[clang::no_sanitize("float-divide-by-zero")]] static inline float
SkityIEEEFloatDivided(float number, float denom) {
  return number / denom;
}

#define FloatInvert(x) SkityIEEEFloatDivided(Float1, (x))

static inline bool FloatIsFinite(float x) { return !glm::isinf(x); }

static inline float FloatSinSnapToZero(float radians) {
  float v = std::sin(radians);
  return FloatNearlyZero(v) ? 0.f : v;
}

static inline float FloatCosSnapToZero(float radians) {
  float v = std::cos(radians);
  return FloatNearlyZero(v) ? 0.f : v;
}

static inline float FloatTanSnapToZero(float radians) {
  float v = std::tan(radians);
  return FloatNearlyZero(v) ? 0.f : v;
}

static inline float FloatCopySign(float v1, float v2) {
  return std::copysignf(v1, v2);
}

static inline Vec2 Times2(Vec2 const& value) { return value + value; }

template <class T>
T Interp(T const& v0, T const& v1, T const& t) {
  return v0 + (v1 - v0) * t;
}

template <class T>
float CrossProduct(T const& a, T const& b) {
  return a.x * b.y - a.y * b.x;
}

enum class Orientation {
  kLinear,
  kClockWise,
  kAntiClockWise,
};

template <class T>
Orientation CalculateOrientation(T const& p, T const& q, T const& r) {
  float val = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);

  if (FloatNearlyZero(val, 0.001f)) {
    return Orientation::kLinear;
  }

  return (val > 0) ? Orientation::kClockWise : Orientation::kAntiClockWise;
}

template <class T>
int32_t CrossProductResult(T const& p, T const& q, T const& r) {
  return (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);
}

//! Returns the number of leading zero bits (0...32)
// From Hacker's Delight 2nd Edition
constexpr int CLZ(uint32_t x) {
  int n = 32;

  uint32_t y = x >> 16;
  if (y != 0) {
    n -= 16;
    x = y;
  }
  y = x >> 8;
  if (y != 0) {
    n -= 8;
    x = y;
  }
  y = x >> 4;
  if (y != 0) {
    n -= 4;
    x = y;
  }
  y = x >> 2;
  if (y != 0) {
    n -= 2;
    x = y;
  }
  y = x >> 1;
  if (y != 0) {
    return n - 2;
  }

  return n - static_cast<int>(x);
}

template <typename T>
constexpr inline bool IsPow2(T value) {
  return (value & (value - 1)) == 0;
}

constexpr int NextLog2(uint32_t value) { return 32 - CLZ(value - 1); }

constexpr int NextPow2(int value) {
  return 1 << NextLog2(static_cast<uint32_t>(value));
}

// Map 'value' to a larger multiple of 2. Values <= 'kMagicTol' will pop up to
// the next power of 2. Those above 'kMagicTol' will only go up half the floor
// power of 2.
inline glm::uvec2 MakeApprox(glm::uvec2 dimensions) {
  auto adjust = [](int value) {
    static const int kMagicTol = 1024;

    value = std::max(16, value);

    if (IsPow2(value)) {
      return value;
    }

    int ceil_pow2 = NextPow2(value);
    if (value <= kMagicTol) {
      return ceil_pow2;
    }

    int floor_pow2 = ceil_pow2 >> 1;
    int mid = floor_pow2 + (floor_pow2 >> 1);

    if (value <= mid) {
      return mid;
    }
    return ceil_pow2;
  };

  return {adjust(dimensions.x), adjust(dimensions.y)};
}

#ifdef __GNUC__
#pragma clang diagnostic pop
#endif

}  // namespace skity

#endif  // SRC_GEOMETRY_MATH_HPP
