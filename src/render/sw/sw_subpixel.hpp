// Copyright 2006 The Android Open Source Project.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_SW_SW_SUBPIXEL_HPP
#define SRC_RENDER_SW_SW_SUBPIXEL_HPP

#include <array>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace skity {

/** 32 bit signed integer used to represent fractions values with 16 bits to the
 * right of the decimal point
 */
typedef int32_t SWFixed;
typedef int32_t SWFDot6;
#define SW_Fixed1 (1 << 16)
#define SW_FixedHalf (1 << 15)
#define SW_FixedQuarter (1 << 14)
#define SW_FixedMax (0x7FFFFFFF)
#define SW_FixedMin (-SK_FixedMax)
#define SW_FixedPI (0x3243F)
#define SW_FixedSqrt2 (92682)
#define SW_FixedTanPIOver8 (0x6A0A)
#define SW_FixedRoot2Over2 (0xB505)

// Max Signed 16 bit value
static constexpr int16_t SK_MaxS16 = INT16_MAX;
static constexpr int16_t SK_MinS16 = -SK_MaxS16;

static constexpr int32_t SW_MaxS32 = INT32_MAX;
static constexpr int32_t SW_MinS32 = -SW_MaxS32;
static constexpr int32_t SW_NaN32 = INT32_MIN;

static inline SWFixed SWFixedMul(SWFixed a, SWFixed b) {
  return (SWFixed)((int64_t)a * b >> 16);
}

inline SWFixed SWFDot6ToFixed(SWFDot6 x) { return x << 10; }

static inline constexpr int32_t SWLeftShift(int32_t value, int32_t shift) {
  return (int32_t)((uint32_t)value << shift);
}

static inline constexpr int64_t SWLeftShift(int64_t value, int32_t shift) {
  return (int64_t)((uint64_t)value << shift);
}

static inline SWFixed SWFDot6ToFixedDiv2(SWFDot6 value) {
  // we want to return SWFDot6ToFixed(value >> 1), but we don't want to throw
  // away data in value, so just perform a modify up-shift
  return SWLeftShift(value, 16 - 6 - 1);
}

template <typename T>
static constexpr const T& SkTPin(const T& x, const T& lo, const T& hi) {
  return std::max(lo, std::min(x, hi));
}

// The divide may exceed 32 bits. Clamp to a signed 32 bit result.
#define SWFixedDiv(numer, denom)        \
  static_cast<int32_t>(SkTPin<int64_t>( \
      (SWLeftShift((int64_t)(numer), 16) / (denom)), SW_MinS32, SW_MaxS32))

inline SWFixed SWFDot6Div(SWFDot6 a, SWFDot6 b) { return SWFixedDiv(a, b); }

#define SWFDot6Round(x) (((x) + 32) >> 6)

#define SkScalarToFDot6(x) (SWFDot6)((x) * 64)
#define SkFixedToFDot6(x) ((x) >> 10)

static inline SWFixed SWFixedRoundToFixed(SWFixed x) {
  return (SWFixed)((uint32_t)(x + SW_FixedHalf) & 0xFFFF0000);
}
static inline SWFixed SWFixedCeilToFixed(SWFixed x) {
  return (SWFixed)((uint32_t)(x + SW_Fixed1 - 1) & 0xFFFF0000);
}
static inline SWFixed SWFixedFloorToFixed(SWFixed x) {
  return (SWFixed)((uint32_t)x & 0xFFFF0000);
}

#define SWIntToFixed(n) (SWFixed)((unsigned)(n) << 16)

#define SWFixedRoundToInt(x) (((x) + SW_FixedHalf) >> 16)
#define SWFixedCeilToInt(x) (((x) + SW_Fixed1 - 1) >> 16)
#define SWFixedFloorToInt(x) ((x) >> 16)

struct Span {
  int32_t x;
  int32_t y;
  int32_t len;
  int32_t cover;
};

}  // namespace skity

#endif  // SRC_RENDER_SW_SW_SUBPIXEL_HPP
