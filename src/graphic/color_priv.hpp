// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_COLOR_PRIV_HPP
#define SRC_GRAPHIC_COLOR_PRIV_HPP

#include <algorithm>
#include <cassert>
#include <skity/graphic/color.hpp>

namespace skity {

/**
 *  Return a*b/((1 << shift) - 1), rounding any fractional bits.
 *  Only valid if a and b are unsigned and <= 32767 and shift is > 0 and <= 8
 */
static inline unsigned Mul16ShiftRound(uint32_t a, uint32_t b, int shift) {
  assert(a <= 32767);
  assert(b <= 32767);
  assert(shift > 0 && shift <= 8);
  unsigned prod = a * b + (1 << (shift - 1));
  return (prod + (prod >> shift)) >> shift;
}

/**
 *  Return a*b/255, rounding any fractional bits.
 *  Only valid if a and b are unsigned and <= 32767.
 */
inline uint32_t MulDiv255Round(uint32_t a, uint32_t b) {
  return Mul16ShiftRound(a, b, 8);
}

static inline PMColor PremultiplyARGBInline(uint32_t a, uint32_t r, uint32_t g,
                                            uint32_t b) {
  assert((r & 0x00FF0000) <= 0x00FF0000);
  assert((g & 0x0000FF00) <= 0x0000FF00);
  assert((b & 0x000000FF) <= 0x000000FF);
  if (a != 255) {
    r = MulDiv255Round(r, a);
    g = MulDiv255Round(g, a);
    b = MulDiv255Round(b, a);
  }
  return (a << 24) | (r << 16) | (g << 8) | (b << 0);
}

// When Android is compiled optimizing for size, SkAlphaMulQ doesn't get
// inlined; forcing inlining significantly improves performance.
static inline uint32_t AlphaMulQ(uint32_t c, unsigned scale) {
  uint32_t mask = 0xFF00FF;

  uint32_t rb = ((c & mask) * scale) >> 8;
  uint32_t ag = ((c >> 8) & mask) * scale;
  return (rb & mask) | (ag & ~mask);
}

// Round after adding both products so complementary terms retain their sum.
static inline PMColor PMColorWeightedSum(PMColor a, uint32_t a_weight,
                                         PMColor b, uint32_t b_weight) {
  auto channel = [=](uint32_t x, uint32_t y) {
    return (x * a_weight + y * b_weight + 127) / 255;
  };
  return ColorSetARGB(
      channel(ColorGetA(a), ColorGetA(b)), channel(ColorGetR(a), ColorGetR(b)),
      channel(ColorGetG(a), ColorGetG(b)), channel(ColorGetB(a), ColorGetB(b)));
}

static inline PMColor ApplyCoverage(PMColor result, PMColor dst,
                                    uint8_t coverage) {
  return coverage == 255
             ? result
             : PMColorWeightedSum(result, coverage, dst, 255 - coverage);
}

static inline PMColor PMColorMul(PMColor src, PMColor dst) {
  return (MulDiv255Round(ColorGetA(src), ColorGetA(dst)) << 24) |
         (MulDiv255Round(ColorGetR(src), ColorGetR(dst)) << 16) |
         (MulDiv255Round(ColorGetG(src), ColorGetG(dst)) << 8) |
         (MulDiv255Round(ColorGetB(src), ColorGetB(dst)));
}

static inline PMColor PMColorScale(PMColor color, uint8_t scale) {
  return PMColorMul(color, ColorSetARGB(scale, scale, scale, scale));
}

static inline PMColor PMColorSwapRB(PMColor color) {
  return ColorSetARGB(ColorGetA(color), ColorGetB(color), ColorGetG(color),
                      ColorGetR(color));
}

}  // namespace skity
#endif  // SRC_GRAPHIC_COLOR_PRIV_HPP
