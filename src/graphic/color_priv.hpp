// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_COLOR_PRIV_HPP
#define SRC_GRAPHIC_COLOR_PRIV_HPP

#include <algorithm>
#include <skity/graphic/color.hpp>

namespace skity {

/** Turn 0..255 into 0..256 by adding 1 at the half-way point. Used to turn a
    byte into a scale value, so that we can say scale * value >> 8 instead of
    alpha * value / 255.

    In debugging, asserts that alpha is 0..255
*/
static inline uint32_t Alpha255To256(uint32_t alpha) {
  assert((alpha & 0xFF) == alpha);
  // this one assues that blending on top of an opaque dst keeps it that way
  // even though it is less accurate than a+(a>>7) for non-opaque dsts
  return alpha + 1;
}

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

static inline PMColor ColorToPMColor(Color c) {
  return PremultiplyARGBInline(ColorGetA(c), ColorGetR(c), ColorGetG(c),
                               ColorGetB(c));
}

Color PMColorToColor(PMColor c);

// When Android is compiled optimizing for size, SkAlphaMulQ doesn't get
// inlined; forcing inlining significantly improves performance.
static inline uint32_t AlphaMulQ(uint32_t c, unsigned scale) {
  uint32_t mask = 0xFF00FF;

  uint32_t rb = ((c & mask) * scale) >> 8;
  uint32_t ag = ((c >> 8) & mask) * scale;
  return (rb & mask) | (ag & ~mask);
}

static inline PMColor PMSrcOver(PMColor src, PMColor dst) {
  return src + AlphaMulQ(dst, Alpha255To256(255 - ColorGetA(src)));
}

static inline PMColor PMColorMul(PMColor src, PMColor dst) {
  return (MulDiv255Round(ColorGetA(src), ColorGetA(dst)) << 24) |
         (MulDiv255Round(ColorGetR(src), ColorGetR(dst)) << 16) |
         (MulDiv255Round(ColorGetG(src), ColorGetG(dst)) << 8) |
         (MulDiv255Round(ColorGetB(src), ColorGetB(dst)));
}

static inline PMColor PMColorSwapRB(PMColor color) {
  return ColorSetARGB(ColorGetA(color), ColorGetB(color), ColorGetG(color),
                      ColorGetR(color));
}

}  // namespace skity
#endif  // SRC_GRAPHIC_COLOR_PRIV_HPP
