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

#include "src/graphic/blend_mode_priv.hpp"
#include "src/graphic/color_priv.hpp"
#include "src/logging.hpp"

#ifdef SKITY_ARM_NEON
#include <array>
#include <cstring>

#include "src/graphic/color_priv_neon.hpp"
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
      return PMSrcOver(src, dst);
    case BlendMode::kDstOver:  // r = d + (1-da)*s
      if (ColorGetA(dst) == 255) {
        return dst;
      }
      return PMSrcOver(dst, src);
    case BlendMode::kSrcIn:  // r = s * da
      return (ColorGetA(dst) == 255)
                 ? src
                 : AlphaMulQ(src, Alpha255To256(ColorGetA(dst)));
    case BlendMode::kDstIn:  // r = d * sa
      if (ColorGetA(src) == 255) {
        return dst;
      }
      return AlphaMulQ(dst, Alpha255To256(ColorGetA(src)));
    case BlendMode::kSrcOut:  // r = s * (1-da)
      return (ColorGetA(dst) == 0)
                 ? src
                 : AlphaMulQ(src, Alpha255To256(255 - ColorGetA(dst)));
    case BlendMode::kDstOut:  // r = d * (1-sa)
      if (ColorGetA(src) == 0) {
        return dst;
      }
      return AlphaMulQ(dst, Alpha255To256(255 - ColorGetA(src)));
    case BlendMode::kSrcATop:  // r = s*da + d*(1-sa)
      return AlphaMulQ(src, Alpha255To256(ColorGetA(dst))) +
             AlphaMulQ(dst, Alpha255To256(255 - ColorGetA(src)));
    case BlendMode::kDstATop:  // r = d*sa + s*(1-da)
      return AlphaMulQ(dst, Alpha255To256(ColorGetA(src))) +
             AlphaMulQ(src, Alpha255To256(255 - ColorGetA(dst)));
    case BlendMode::kXor:  // r = s*(1-da) + d*(1-sa)
      return AlphaMulQ(src, Alpha255To256(255 - ColorGetA(dst))) +
             AlphaMulQ(dst, Alpha255To256(255 - ColorGetA(src)));
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

void PMColorSwapRB(uint32_t* colors, uint32_t count) {
  int32_t i_count = static_cast<int32_t>(count);

  while (i_count >= 8) {
    uint8x8x4_t rgba = vld4_u8(reinterpret_cast<const uint8_t*>(colors));

    // swap rb
    std::swap(rgba.val[0], rgba.val[2]);

    vst4_u8(reinterpret_cast<uint8_t*>(colors), rgba);
    colors += 8;
    i_count -= 8;
  }

  while (i_count > 0) {
    auto color = *colors;
    *colors = ColorSetARGB(ColorGetA(color), ColorGetB(color), ColorGetG(color),
                           ColorGetR(color));
    i_count--;

    colors++;
  }
}

void ProterDuffBlendNeon(uint32_t* src, uint32_t* dst, uint32_t len,
                         BlendMode mode) {
  PMColorSwapRB(src, len);

  if (mode == BlendMode::kClear) {
    std::memset(dst, 0, len * 4);
  } else if (mode == BlendMode::kSrc) {
    std::memcpy(dst, src, len * 4);
  } else if (mode == BlendMode::kDst) {
    return;
  } else {
    uint32_t iterations = len / 8;
    uint32_t neon_filled = iterations * 8;

    for (uint32_t i = 0; i < iterations; i++) {
      if (mode == BlendMode::kSrcOver) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcOverNeon(vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                              vld4_u8(reinterpret_cast<const uint8_t*>(src))));
      } else if (mode == BlendMode::kDstOver) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcOverNeon(vld4_u8(reinterpret_cast<const uint8_t*>(src)),
                              vld4_u8(reinterpret_cast<const uint8_t*>(dst))));
      } else if (mode == BlendMode::kSrcIn) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcInNeon(vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                            vld4_u8(reinterpret_cast<const uint8_t*>(src))));
      } else if (mode == BlendMode::kDstIn) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcInNeon(vld4_u8(reinterpret_cast<const uint8_t*>(src)),
                            vld4_u8(reinterpret_cast<const uint8_t*>(dst))));
      } else if (mode == BlendMode::kSrcOut) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcOutNeon(vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                             vld4_u8(reinterpret_cast<const uint8_t*>(src))));
      } else if (mode == BlendMode::kDstOut) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcOutNeon(vld4_u8(reinterpret_cast<const uint8_t*>(src)),
                             vld4_u8(reinterpret_cast<const uint8_t*>(dst))));
      } else if (mode == BlendMode::kSrcATop) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcATopNeon(vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                              vld4_u8(reinterpret_cast<const uint8_t*>(src))));
      } else if (mode == BlendMode::kDstATop) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcATopNeon(vld4_u8(reinterpret_cast<const uint8_t*>(src)),
                              vld4_u8(reinterpret_cast<const uint8_t*>(dst))));
      } else if (mode == BlendMode::kXor) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMXorNeon(vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                          vld4_u8(reinterpret_cast<const uint8_t*>(src))));
      } else if (mode == BlendMode::kPlus) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMPlusNeon(vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                           vld4_u8(reinterpret_cast<const uint8_t*>(src))));
      } else if (mode == BlendMode::kModulate) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMModulateNeon(vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                               vld4_u8(reinterpret_cast<const uint8_t*>(src))));
      } else if (mode == BlendMode::kScreen) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMScreenNeon(vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                             vld4_u8(reinterpret_cast<const uint8_t*>(src))));
      }

      dst += 8;
      src += 8;
    }

    int32_t left_overs = len - neon_filled;
    while (left_overs--) {
      *dst = PorterDuffBlend(*src, *dst, mode);
      dst++;
      src++;
    }
  }
}

void ProterDuffBlendNeon(uint32_t src, uint32_t* dst, uint32_t len,
                         BlendMode mode) {
  src = PMColorSwapRB(src);

  if (mode == BlendMode::kClear) {
    std::memset(dst, 0, len * 4);
  } else if (mode == BlendMode::kSrc) {
    uint32_t iterations = len / 4;
    uint32_t neon_filled = iterations * 4;
    uint32x4_t vectorVal = {src, src, src, src};

    for (uint32_t i = 0; i < iterations; ++i) {
      vst1q_u32(dst, vectorVal);
      dst += 4;
    }

    int32_t leftovers = len - neon_filled;
    while (leftovers--) *dst++ = src;

  } else if (mode == BlendMode::kDst) {
    return;
  } else {
    uint32_t iterations = len / 8;
    uint32_t neon_filled = iterations * 8;

    std::array<PMColor, 8> pm_colors{
        src, src, src, src, src, src, src, src,
    };

    for (uint32_t i = 0; i < iterations; i++) {
      if (mode == BlendMode::kSrcOver) {
        vst4_u8(
            reinterpret_cast<uint8_t*>(dst),
            PMSrcOverNeon(
                vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data()))));
      } else if (mode == BlendMode::kDstOver) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcOverNeon(
                    vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data())),
                    vld4_u8(reinterpret_cast<const uint8_t*>(dst))));
      } else if (mode == BlendMode::kSrcIn) {
        vst4_u8(
            reinterpret_cast<uint8_t*>(dst),
            PMSrcInNeon(
                vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data()))));
      } else if (mode == BlendMode::kDstIn) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcInNeon(
                    vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data())),
                    vld4_u8(reinterpret_cast<const uint8_t*>(dst))));
      } else if (mode == BlendMode::kSrcOut) {
        vst4_u8(
            reinterpret_cast<uint8_t*>(dst),
            PMSrcOutNeon(
                vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data()))));
      } else if (mode == BlendMode::kDstOut) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcOutNeon(
                    vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data())),
                    vld4_u8(reinterpret_cast<const uint8_t*>(dst))));
      } else if (mode == BlendMode::kSrcATop) {
        vst4_u8(
            reinterpret_cast<uint8_t*>(dst),
            PMSrcATopNeon(
                vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data()))));
      } else if (mode == BlendMode::kDstATop) {
        vst4_u8(reinterpret_cast<uint8_t*>(dst),
                PMSrcATopNeon(
                    vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data())),
                    vld4_u8(reinterpret_cast<const uint8_t*>(dst))));
      } else if (mode == BlendMode::kXor) {
        vst4_u8(
            reinterpret_cast<uint8_t*>(dst),
            PMXorNeon(
                vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data()))));
      } else if (mode == BlendMode::kPlus) {
        vst4_u8(
            reinterpret_cast<uint8_t*>(dst),
            PMPlusNeon(
                vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data()))));
      } else if (mode == BlendMode::kModulate) {
        vst4_u8(
            reinterpret_cast<uint8_t*>(dst),
            PMModulateNeon(
                vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data()))));
      } else if (mode == BlendMode::kScreen) {
        vst4_u8(
            reinterpret_cast<uint8_t*>(dst),
            PMScreenNeon(
                vld4_u8(reinterpret_cast<const uint8_t*>(dst)),
                vld4_u8(reinterpret_cast<const uint8_t*>(pm_colors.data()))));
      }

      dst += 8;
    }

    int32_t left_overs = len - neon_filled;
    while (left_overs--) {
      *dst = PorterDuffBlend(src, *dst, mode);
      dst++;
    }
  }
}

#endif

}  // namespace skity
