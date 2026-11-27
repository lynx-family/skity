// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_COLOR_PRIV_SSE2_HPP
#define SRC_GRAPHIC_COLOR_PRIV_SSE2_HPP

#include <emmintrin.h>

namespace skity {
namespace sse2 {

// Four packed pixels, with the alpha byte last in both RGBA and BGRA.
inline __m128i Alpha(__m128i pixels) {
  auto a = _mm_srli_epi32(pixels, 24);
  a = _mm_or_si128(a, _mm_slli_epi32(a, 8));
  return _mm_or_si128(a, _mm_slli_epi32(a, 16));
}

inline __m128i SwapRB(__m128i pixels) {
  auto diff = _mm_and_si128(_mm_xor_si128(pixels, _mm_srli_epi32(pixels, 16)),
                            _mm_set1_epi32(255));
  return _mm_xor_si128(pixels, _mm_or_si128(diff, _mm_slli_epi32(diff, 16)));
}

inline __m128i MultiplyLowBytes(__m128i x, __m128i y) {
  const auto zero = _mm_setzero_si128();
  return _mm_mullo_epi16(_mm_unpacklo_epi8(x, zero),
                         _mm_unpacklo_epi8(y, zero));
}

inline __m128i MultiplyHighBytes(__m128i x, __m128i y) {
  const auto zero = _mm_setzero_si128();
  return _mm_mullo_epi16(_mm_unpackhi_epi8(x, zero),
                         _mm_unpackhi_epi8(y, zero));
}

inline __m128i Div255Round(__m128i product) {
  auto biased = _mm_add_epi16(product, _mm_set1_epi16(128));
  return _mm_srli_epi16(_mm_add_epi16(biased, _mm_srli_epi16(biased, 8)), 8);
}

inline __m128i MulDiv255Round(__m128i x, __m128i y) {
  return _mm_packus_epi16(Div255Round(MultiplyLowBytes(x, y)),
                          Div255Round(MultiplyHighBytes(x, y)));
}

inline __m128i WeightedSum(__m128i a, __m128i aw, __m128i b, __m128i bw) {
  // Round once, after adding the two premultiplied terms.
  auto lo = _mm_add_epi16(MultiplyLowBytes(a, aw), MultiplyLowBytes(b, bw));
  auto hi = _mm_add_epi16(MultiplyHighBytes(a, aw), MultiplyHighBytes(b, bw));
  return _mm_packus_epi16(Div255Round(lo), Div255Round(hi));
}

}  // namespace sse2
}  // namespace skity

#endif  // SRC_GRAPHIC_COLOR_PRIV_SSE2_HPP
