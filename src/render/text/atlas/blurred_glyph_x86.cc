// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/blurred_glyph.hpp"

#ifdef SKITY_GLYPH_BLUR_X64
#include <immintrin.h>
#ifdef SKITY_WIN
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#include "src/base/platform/win/lean_windows.hpp"
#endif

#include <algorithm>
#include <cstring>

namespace skity {
namespace {

// Coverage is nonnegative. Adding 0.5f before truncation would incorrectly
// round the float immediately below 0.5f up to 1.
__m128i RoundCoverage(__m128 value) {
  const auto integer = _mm_cvttps_epi32(value);
  const auto fraction = _mm_sub_ps(value, _mm_cvtepi32_ps(integer));
  const auto round_up = _mm_cmpge_ps(fraction, _mm_set1_ps(0.5f));
  return _mm_sub_epi32(integer, _mm_castps_si128(round_up));
}

void HorizontalSSE2(const uint8_t* src, bool gray, int width, float* alpha,
                    const float* weights, int taps, float* dst) {
  for (int x = 0; x < width; ++x) {
    alpha[x] = gray ? src[x] : src[4 * x + 3];
  }
  for (int k = 0; k < taps; ++k) {
    const auto weight = _mm_set1_ps(weights[k]);
    for (int x = 0; x < width; x += 4) {
      const auto product = _mm_mul_ps(_mm_loadu_ps(alpha + x), weight);
      _mm_storeu_ps(dst + k + x,
                    _mm_add_ps(_mm_loadu_ps(dst + k + x), product));
    }
  }
}

void VerticalSSE2(const float* src, int stride, const float* weights, int rows,
                  int width, uint8_t* dst) {
  for (int x = 0; x < width; x += 16) {
    __m128 sums[4] = {};
    for (int y = 0; y < rows; ++y) {
      const float* row = src + y * stride + x;
      const auto weight = _mm_set1_ps(weights[-y]);
      for (int i = 0; i < 4; ++i) {
        const auto product = _mm_mul_ps(_mm_loadu_ps(row + 4 * i), weight);
        sums[i] = _mm_add_ps(sums[i], product);
      }
    }
    const auto low =
        _mm_packs_epi32(RoundCoverage(sums[0]), RoundCoverage(sums[1]));
    const auto high =
        _mm_packs_epi32(RoundCoverage(sums[2]), RoundCoverage(sums[3]));
    const auto bytes = _mm_packus_epi16(low, high);
    if (width - x >= 16) {
      _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + x), bytes);
    } else {
      std::memcpy(dst + x, &bytes, width - x);
    }
  }
}

// MSVC emits explicit AVX2 intrinsics even at the x64 baseline. Clang and GCC
// need a function target. Keep these rows out of the baseline dispatcher.
#if defined(__clang__) || defined(__GNUC__)
#define SKITY_AVX2_ROW __attribute__((target("avx2"), noinline))
#define SKITY_AVX2_INLINE inline __attribute__((target("avx2"), always_inline))
#else
#define SKITY_AVX2_ROW __declspec(noinline)
#define SKITY_AVX2_INLINE __forceinline
#endif

SKITY_AVX2_INLINE __m256i RoundCoverage(__m256 value) {
  const auto integer = _mm256_cvttps_epi32(value);
  const auto fraction = _mm256_sub_ps(value, _mm256_cvtepi32_ps(integer));
  const auto round_up =
      _mm256_cmp_ps(fraction, _mm256_set1_ps(0.5f), _CMP_GE_OQ);
  return _mm256_sub_epi32(integer, _mm256_castps_si256(round_up));
}

SKITY_AVX2_ROW void HorizontalAVX2(const uint8_t* src, bool gray, int width,
                                   float* alpha, const float* weights, int taps,
                                   float* dst) {
  int x = 0;
  for (; x + 8 <= width; x += 8) {
    __m256i coverage;
    if (gray) {
      const auto bytes =
          _mm_loadl_epi64(reinterpret_cast<const __m128i*>(src + x));
      coverage = _mm256_cvtepu8_epi32(bytes);
    } else {
      const auto colors =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + 4 * x));
      coverage = _mm256_srli_epi32(colors, 24);
    }
    _mm256_storeu_ps(alpha + x, _mm256_cvtepi32_ps(coverage));
  }
  for (; x < width; ++x) {
    alpha[x] = gray ? src[x] : src[4 * x + 3];
  }
  for (int k = 0; k < taps; ++k) {
    const auto weight = _mm256_set1_ps(weights[k]);
    for (int i = 0; i < width; i += 8) {
      const auto product = _mm256_mul_ps(_mm256_loadu_ps(alpha + i), weight);
      _mm256_storeu_ps(dst + k + i,
                       _mm256_add_ps(_mm256_loadu_ps(dst + k + i), product));
    }
  }
}

SKITY_AVX2_ROW void VerticalAVX2(const float* src, int stride,
                                 const float* weights, int rows, int width,
                                 uint8_t* dst) {
  for (int x = 0; x < width; x += 32) {
    const auto zero = _mm256_setzero_ps();
    __m256 sums[4] = {zero, zero, zero, zero};
    for (int y = 0; y < rows; ++y) {
      const float* row = src + y * stride + x;
      const auto weight = _mm256_set1_ps(weights[-y]);
      for (int i = 0; i < 4; ++i) {
        const auto product =
            _mm256_mul_ps(_mm256_loadu_ps(row + 8 * i), weight);
        sums[i] = _mm256_add_ps(sums[i], product);
      }
    }
    const auto low =
        _mm256_packs_epi32(RoundCoverage(sums[0]), RoundCoverage(sums[1]));
    const auto high =
        _mm256_packs_epi32(RoundCoverage(sums[2]), RoundCoverage(sums[3]));
    // Packing works within 128-bit halves. Restore increasing pixel order.
    const auto bytes =
        _mm256_permutevar8x32_epi32(_mm256_packus_epi16(low, high),
                                    _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7));
    if (width - x >= 32) {
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + x), bytes);
    } else {
      std::memcpy(dst + x, &bytes, width - x);
    }
  }
}

#undef SKITY_AVX2_ROW
#undef SKITY_AVX2_INLINE

bool SupportsAVX2() {
#ifdef SKITY_LINUX
  // GCC and Clang check both the CPU feature and OS-enabled XMM/YMM state.
  return __builtin_cpu_supports("avx2");
#else
  // This Windows query also supports versions of Windows 10 that predate
  // PF_AVX2_INSTRUCTIONS_AVAILABLE.
  constexpr auto kAVXState = XSTATE_MASK_LEGACY_SSE | XSTATE_MASK_AVX;
  if ((GetEnabledXStateFeatures() & kAVXState) != kAVXState) {
    return false;
  }
  constexpr int kAVX2 = 1 << 5;  // CPUID.07H:EBX
#ifdef _MSC_VER
  int regs[4];
  __cpuid(regs, 0);
  if (regs[0] < 7) {
    return false;
  }
  __cpuidex(regs, 7, 0);
  return (regs[1] & kAVX2) != 0;
#else
  unsigned int eax, ebx, ecx, edx;
  return __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) && (ebx & kAVX2) != 0;
#endif
#endif
}

struct GlyphBlurRows {
  int lanes;
  decltype(&HorizontalSSE2) horizontal;
  decltype(&VerticalSSE2) vertical;
};

const GlyphBlurRows& GetBlurRows() {
  static const GlyphBlurRows rows =
      SupportsAVX2() ? GlyphBlurRows{8, HorizontalAVX2, VerticalAVX2}
                     : GlyphBlurRows{4, HorizontalSSE2, VerticalSSE2};
  return rows;
}

}  // namespace

std::vector<uint8_t> BlurGlyphMask(const GlyphBitmapData& source,
                                   const std::vector<float>& weights_x,
                                   const std::vector<float>& weights_y) {
  const auto& kernel = GetBlurRows();
  const int width = static_cast<int>(source.width);
  const int height = static_cast<int>(source.height);
  const int taps_x = static_cast<int>(weights_x.size());
  const int taps_y = static_cast<int>(weights_y.size());
  const int output_width = width + taps_x - 1;
  const int output_height = height + taps_y - 1;
  // Pad only our scratch rows. The source may end at its last valid pixel.
  const int alpha_width = (width + kernel.lanes - 1) & ~(kernel.lanes - 1);
  const int block = 4 * kernel.lanes;
  const int stride = (alpha_width + taps_x - 1 + block - 1) & ~(block - 1);
  std::vector<float> alpha(alpha_width);
  std::vector<float> horizontal(static_cast<size_t>(stride) * height);
  const size_t row_bytes = source.RowBytes();
  const bool gray = source.format == BitmapFormat::kGray8;
  for (int y = 0; y < height; ++y) {
    kernel.horizontal(source.buffer + y * row_bytes, gray, width, alpha.data(),
                      weights_x.data(), taps_x, horizontal.data() + y * stride);
  }

  std::vector<uint8_t> pixels(static_cast<size_t>(output_width) *
                              output_height);
  for (int y = 0; y < output_height; ++y) {
    const int first = std::max(0, y - taps_y + 1);
    const int last = std::min(y, height - 1);
    kernel.vertical(horizontal.data() + first * stride, stride,
                    weights_y.data() + (y - first), last - first + 1,
                    output_width, pixels.data() + y * output_width);
  }
  return pixels;
}

}  // namespace skity
#endif  // SKITY_GLYPH_BLUR_X64
