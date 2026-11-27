/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <algorithm>
#include <cmath>
#include <cstring>

#include "src/graphic/blend_mode_priv.hpp"
#include "src/graphic/color_priv.hpp"
#include "src/logging.hpp"

#ifdef SKITY_ARM_NEON
#include "src/graphic/color_priv_neon.hpp"
#elif defined(SKITY_X86_SSE2)
#include "src/graphic/color_priv_sse2.hpp"
#endif

namespace skity {

const char* BlendMode_Name(BlendMode bm) {
  switch (bm) {
    case BlendMode::kClear:
      return "Clear";
    case BlendMode::kSrc:
      return "Src";
    case BlendMode::kDst:
      return "Dst";
    case BlendMode::kSrcOver:
      return "SrcOver";
    case BlendMode::kDstOver:
      return "DstOver";
    case BlendMode::kSrcIn:
      return "SrcIn";
    case BlendMode::kDstIn:
      return "DstIn";
    case BlendMode::kSrcOut:
      return "SrcOut";
    case BlendMode::kDstOut:
      return "DstOut";
    case BlendMode::kSrcATop:
      return "SrcATop";
    case BlendMode::kDstATop:
      return "DstATop";
    case BlendMode::kXor:
      return "Xor";
    case BlendMode::kPlus:
      return "Plus";
    case BlendMode::kModulate:
      return "Modulate";
    case BlendMode::kScreen:
      return "Screen";

    case BlendMode::kOverlay:
      return "Overlay";
    case BlendMode::kDarken:
      return "Darken";
    case BlendMode::kLighten:
      return "Lighten";
    case BlendMode::kColorDodge:
      return "ColorDodge";
    case BlendMode::kColorBurn:
      return "ColorBurn";
    case BlendMode::kHardLight:
      return "HardLight";
    case BlendMode::kSoftLight:
      return "SoftLight";
    case BlendMode::kDifference:
      return "Difference";
    case BlendMode::kExclusion:
      return "Exclusion";
    case BlendMode::kMultiply:
      return "Multiply";

    case BlendMode::kHue:
      return "Hue";
    case BlendMode::kSaturation:
      return "Saturation";
    case BlendMode::kColor:
      return "Color";
    case BlendMode::kLuminosity:
      return "Luminosity";
    default:
      return "Unknown";
  }
}

static float HardLight(float s, float d) {
  return s <= 0.5f ? 2.f * s * d : 1.f - 2.f * (1.f - s) * (1.f - d);
}

static float BlendChannel(float s, float d, BlendMode mode) {
  switch (mode) {
    case BlendMode::kOverlay:
      return HardLight(d, s);
    case BlendMode::kDarken:
      return std::min(s, d);
    case BlendMode::kLighten:
      return std::max(s, d);
    case BlendMode::kColorDodge:
      if (d == 0.f) {
        return 0.f;
      }
      return s == 1.f ? 1.f : std::min(1.f, d / (1.f - s));
    case BlendMode::kColorBurn:
      if (d == 1.f) {
        return 1.f;
      }
      return s == 0.f ? 0.f : 1.f - std::min(1.f, (1.f - d) / s);
    case BlendMode::kHardLight:
      return HardLight(s, d);
    case BlendMode::kSoftLight: {
      if (s <= 0.5f) {
        return d - (1.f - 2.f * s) * d * (1.f - d);
      }
      float curve =
          d <= 0.25f ? ((16.f * d - 12.f) * d + 4.f) * d : std::sqrt(d);
      return d + (2.f * s - 1.f) * (curve - d);
    }
    case BlendMode::kDifference:
      return std::abs(d - s);
    case BlendMode::kExclusion:
      return s + d - 2.f * s * d;
    default:  // Multiply
      return s * d;
  }
}

static float Luminosity(Vec3 c) { return Vec3::Dot(c, {0.3f, 0.59f, 0.11f}); }

static float Saturation(Vec3 c) {
  return std::max({c.x, c.y, c.z}) - std::min({c.x, c.y, c.z});
}

static Vec3 SetSaturation(Vec3 c, float saturation) {
  float range = Saturation(c);
  float low = std::min({c.x, c.y, c.z});
  return range > 0.f ? (c - Vec3{low, low, low}) * (saturation / range)
                     : Vec3{};
}

static Vec3 SetLuminosity(Vec3 c, float lum) {
  float delta = lum - Luminosity(c);
  c += Vec3{delta, delta, delta};
  float low = std::min({c.x, c.y, c.z});
  float high = std::max({c.x, c.y, c.z});
  Vec3 gray{lum, lum, lum};
  if (low < 0.f) {
    c = gray + (c - gray) * (lum / (lum - low));
  }
  if (high > 1.f) {
    c = gray + (c - gray) * ((1.f - lum) / (high - lum));
  }
  return c;
}

static Vec3 BlendRGB(Vec3 s, Vec3 d, BlendMode mode) {
  switch (mode) {
    case BlendMode::kHue:
      return SetLuminosity(SetSaturation(s, Saturation(d)), Luminosity(d));
    case BlendMode::kSaturation:
      return SetLuminosity(SetSaturation(d, Saturation(s)), Luminosity(d));
    case BlendMode::kColor:
      return SetLuminosity(s, Luminosity(d));
    case BlendMode::kLuminosity:
      return SetLuminosity(d, Luminosity(s));
    default:
      return {BlendChannel(s.x, d.x, mode), BlendChannel(s.y, d.y, mode),
              BlendChannel(s.z, d.z, mode)};
  }
}

static PMColor AdvancedBlend(PMColor src, PMColor dst, BlendMode mode) {
  if (!ColorGetA(src)) {
    return dst;
  }
  if (!ColorGetA(dst)) {
    return src;
  }
  auto s = Color4fFromColor(src);
  auto d = Color4fFromColor(dst);
  Vec3 sc{s.r, s.g, s.b};
  Vec3 dc{d.r, d.g, d.b};
  // Only the intersection of the source and destination uses the blend
  // function.
  Vec3 rgb = sc * (1.f - d.a) + dc * (1.f - s.a) +
             BlendRGB(sc / s.a, dc / d.a, mode) * (s.a * d.a);
  return Color4fToColor({rgb.x, rgb.y, rgb.z, s.a + d.a * (1.f - s.a)});
}

PMColor PorterDuffBlend(PMColor src, PMColor dst, BlendMode mode) {
  if (IsAdvancedBlendMode(mode)) {
    if (mode <= BlendMode::kLastMode) {
      return AdvancedBlend(src, dst, mode);
    }
    LOGE("PorterDuffBlend does not support BlendMode {}, fallback to {}",
         BlendMode_Name(mode), BlendMode_Name(BlendMode::kDefault));
    mode = BlendMode::kDefault;
  }
  switch (mode) {
    case BlendMode::kClear:  // r = 0
      return Color_TRANSPARENT;
    case BlendMode::kSrc:  // r = s
      return src;
    case BlendMode::kDst:  // r = d
      return dst;
    case BlendMode::kSrcOver:  // r = s + (1-sa)*d
      if (ColorGetA(src) == 0) {
        return dst;
      }
      return src + PMColorScale(dst, 255 - ColorGetA(src));
    case BlendMode::kDstOver:  // r = d + (1-da)*s
      if (ColorGetA(dst) == 255) {
        return dst;
      }
      return dst + PMColorScale(src, 255 - ColorGetA(dst));
    case BlendMode::kSrcIn:  // r = s * da
      return PMColorScale(src, ColorGetA(dst));
    case BlendMode::kDstIn:  // r = d * sa
      return PMColorScale(dst, ColorGetA(src));
    case BlendMode::kSrcOut:  // r = s * (1-da)
      return PMColorScale(src, 255 - ColorGetA(dst));
    case BlendMode::kDstOut:  // r = d * (1-sa)
      return PMColorScale(dst, 255 - ColorGetA(src));
    case BlendMode::kSrcATop:  // r = s*da + d*(1-sa)
      return PMColorWeightedSum(src, ColorGetA(dst), dst, 255 - ColorGetA(src));
    case BlendMode::kDstATop:  // r = d*sa + s*(1-da)
      return PMColorWeightedSum(dst, ColorGetA(src), src, 255 - ColorGetA(dst));
    case BlendMode::kXor:  // r = s*(1-da) + d*(1-sa)
      return PMColorWeightedSum(src, 255 - ColorGetA(dst), dst,
                                255 - ColorGetA(src));
    case BlendMode::kPlus:  // r = min(s + d, 1)
      return ColorSetARGB(std::min(ColorGetA(src) + ColorGetA(dst), 255u),
                          std::min(ColorGetR(src) + ColorGetR(dst), 255u),
                          std::min(ColorGetG(src) + ColorGetG(dst), 255u),
                          std::min(ColorGetB(src) + ColorGetB(dst), 255u));
    case BlendMode::kModulate:  // r = s*d
      return PMColorMul(src, dst);
    case BlendMode::kScreen:  // r = s + d - s*d
      return src + dst - PMColorMul(src, dst);
    default:
      return dst;
  }
}

#ifdef SKITY_ARM_NEON

static uint8x8x4_t BlendEightPixels(uint8x8x4_t src, uint8x8x4_t dst,
                                    BlendMode mode) {
  switch (mode) {
    case BlendMode::kClear:
      return {};
    case BlendMode::kSrc:
      return src;
    case BlendMode::kSrcOver:
      return PMSrcOverNeon(dst, src);
    case BlendMode::kDstOver:
      return PMSrcOverNeon(src, dst);
    case BlendMode::kSrcIn:
      return PMSrcInNeon(dst, src);
    case BlendMode::kDstIn:
      return PMSrcInNeon(src, dst);
    case BlendMode::kSrcOut:
      return PMSrcOutNeon(dst, src);
    case BlendMode::kDstOut:
      return PMSrcOutNeon(src, dst);
    case BlendMode::kSrcATop:
      return PMSrcATopNeon(dst, src);
    case BlendMode::kDstATop:
      return PMSrcATopNeon(src, dst);
    case BlendMode::kXor:
      return PMXorNeon(dst, src);
    case BlendMode::kPlus:
      return PMPlusNeon(dst, src);
    case BlendMode::kModulate:
      return PMModulateNeon(dst, src);
    case BlendMode::kScreen:
      return PMScreenNeon(dst, src);
    default:
      return dst;
  }
}

static constexpr uint32_t kBlendBlockSize = 8;

static void BlendBlock(const PMColor* src, bool solid, uint32_t* dst,
                       BlendMode mode, bool swap_rb, uint8_t coverage) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(src);
  auto colors = solid ? vld4_dup_u8(bytes) : vld4_u8(bytes);
  if (swap_rb) {
    std::swap(colors.val[0], colors.val[2]);
  }
  auto* pixels = reinterpret_cast<uint8_t*>(dst);
  auto original = vld4_u8(pixels);
  auto result = BlendEightPixels(colors, original, mode);
  if (coverage != 255) {
    result = PMColorWeightedSumNeon(result, vdup_n_u8(coverage), original,
                                    vdup_n_u8(255 - coverage));
  }
  vst4_u8(pixels, result);
}

#elif defined(SKITY_X86_SSE2)

static __m128i BlendFourPixels(__m128i src, __m128i dst, BlendMode mode) {
  const auto ones = _mm_set1_epi8(-1);
  const auto sa = sse2::Alpha(src);
  const auto da = sse2::Alpha(dst);
  const auto isa = _mm_xor_si128(sa, ones);
  const auto ida = _mm_xor_si128(da, ones);
  switch (mode) {
    case BlendMode::kClear:
      return _mm_setzero_si128();
    case BlendMode::kSrc:
      return src;
    case BlendMode::kSrcOver:
      return _mm_adds_epu8(src, sse2::MulDiv255Round(dst, isa));
    case BlendMode::kDstOver:
      return _mm_adds_epu8(dst, sse2::MulDiv255Round(src, ida));
    case BlendMode::kSrcIn:
      return sse2::MulDiv255Round(src, da);
    case BlendMode::kDstIn:
      return sse2::MulDiv255Round(dst, sa);
    case BlendMode::kSrcOut:
      return sse2::MulDiv255Round(src, ida);
    case BlendMode::kDstOut:
      return sse2::MulDiv255Round(dst, isa);
    case BlendMode::kSrcATop:
      return sse2::WeightedSum(src, da, dst, isa);
    case BlendMode::kDstATop:
      return sse2::WeightedSum(dst, sa, src, ida);
    case BlendMode::kXor:
      return sse2::WeightedSum(src, ida, dst, isa);
    case BlendMode::kPlus:
      return _mm_adds_epu8(src, dst);
    case BlendMode::kModulate:
      return sse2::MulDiv255Round(src, dst);
    case BlendMode::kScreen:
      return _mm_xor_si128(ones,
                           sse2::MulDiv255Round(_mm_xor_si128(src, ones),
                                                _mm_xor_si128(dst, ones)));
    default:
      return dst;
  }
}

static constexpr uint32_t kBlendBlockSize = 4;

static void BlendBlock(const PMColor* src, bool solid, uint32_t* dst,
                       BlendMode mode, bool swap_rb, uint8_t coverage) {
  auto colors = solid ? _mm_set1_epi32(*src)
                      : _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
  if (swap_rb) {
    colors = sse2::SwapRB(colors);
  }
  auto* pixels = reinterpret_cast<__m128i*>(dst);
  const auto original = _mm_loadu_si128(pixels);
  auto result = BlendFourPixels(colors, original, mode);
  if (coverage != 255) {
    result = sse2::WeightedSum(result, _mm_set1_epi8(coverage), original,
                               _mm_set1_epi8(255 - coverage));
  }
  _mm_storeu_si128(pixels, result);
}
#endif

#if defined(SKITY_ARM_NEON) || defined(SKITY_X86_SSE2)
static void BlendRow(const PMColor* src, bool solid, uint32_t* dst,
                     uint32_t len, BlendMode mode, ColorType dst_color_type,
                     uint8_t coverage) {
  if (len == 0 || coverage == 0 || mode == BlendMode::kDst) {
    return;
  }
  const bool swap_rb = dst_color_type == ColorType::kRGBA;
  if (coverage == 255 && mode == BlendMode::kClear) {
    std::fill_n(dst, len, Color_TRANSPARENT);
    return;
  }
  if (coverage == 255 && mode == BlendMode::kSrc) {
    if (solid) {
      std::fill_n(dst, len, swap_rb ? PMColorSwapRB(*src) : *src);
      return;
    }
    if (!swap_rb) {
      std::memcpy(dst, src, len * sizeof(PMColor));
      return;
    }
  }

  while (len >= kBlendBlockSize) {
    BlendBlock(src, solid, dst, mode, swap_rb, coverage);
    if (!solid) {
      src += kBlendBlockSize;
    }
    dst += kBlendBlockSize;
    len -= kBlendBlockSize;
  }
  while (len--) {
    PMColor color = swap_rb ? PMColorSwapRB(*src) : *src;
    *dst = ApplyCoverage(PorterDuffBlend(color, *dst, mode), *dst, coverage);
    if (!solid) {
      ++src;
    }
    ++dst;
  }
}

void PorterDuffBlendRow(const PMColor* src, uint32_t* dst, uint32_t len,
                        BlendMode mode, ColorType dst_color_type,
                        uint8_t coverage) {
  BlendRow(src, false, dst, len, mode, dst_color_type, coverage);
}

void PorterDuffBlendRow(PMColor src, uint32_t* dst, uint32_t len,
                        BlendMode mode, ColorType dst_color_type,
                        uint8_t coverage) {
  if (mode == BlendMode::kSrcOver && ColorGetA(src) == 255) {
    mode = BlendMode::kSrc;
  }
  BlendRow(&src, true, dst, len, mode, dst_color_type, coverage);
}

#endif

}  // namespace skity
