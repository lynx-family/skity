/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

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

static float SoftLightComponent(Vec2 s, Vec2 d) {
  if (2.f * s.x <= s.y) {
    return d.x * d.x * (s.y - 2 * s.x) / d.y + (1 - d.y) * s.x +
           d.x * (-s.y + 2 * s.x + 1);
  } else if (4.f * d.x <= d.y) {
    float DSqd = d.x * d.x;
    float DCub = DSqd * d.x;
    float DaSqd = d.y * d.y;
    float DaCub = DaSqd * d.y;
    return (DaSqd * (s.x - d.x * (3 * s.y - 6 * s.x - 1)) +
            12 * d.y * DSqd * (s.y - 2 * s.x) - 16 * DCub * (s.y - 2 * s.x) -
            DaCub * s.x) /
           DaSqd;
  } else {
    return d.x * (s.y - 2 * s.x + 1) + s.x -
           std::sqrt(d.y * d.x) * (s.y - 2 * s.x) - d.y * s.x;
  }
}

static PMColor SoftLight(PMColor src, PMColor dst) {
  if (ColorGetA(dst) == 0) {
    return src;
  }

  Color4f src_4f = Color4fFromColor(src);
  Color4f dst_4f = Color4fFromColor(dst);
  float r = SoftLightComponent({src_4f.r, src_4f.a}, {dst_4f.r, dst_4f.a});
  float g = SoftLightComponent({src_4f.g, src_4f.a}, {dst_4f.g, dst_4f.a});
  float b = SoftLightComponent({src_4f.b, src_4f.a}, {dst_4f.b, dst_4f.a});
  float a = src_4f.a + (1 - src_4f.a) * dst_4f.a;
  return Color4fToColor({r, g, b, a});
}

PMColor PorterDuffBlend(PMColor src, PMColor dst, BlendMode mode) {
  // Note: Porter-Duff modes are only defined to work properly with
  // premultiplied alpha. That means that none of the R, G or B components can
  // exceed the alpha value.
  if (mode > BlendMode::kScreen && mode != BlendMode::kSoftLight) {
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
    case BlendMode::kSoftLight:
      return SoftLight(src, dst);
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
