// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/color_xform.hpp"

#include <algorithm>
#include <cstring>
#include <skity/io/pixmap.hpp>

#include "src/graphic/color_space_priv.hpp"

namespace skity {

namespace {

// Matches the Color4fToColor quantization: truncate after clamping.
uint8_t Quantize8(float v) {
  return static_cast<uint8_t>(std::clamp(v * 255.f, 0.f, 255.f));
}

}  // namespace

ColorXformSteps ColorXformSteps::Resolve(
    const std::shared_ptr<const ColorSpace>& src,
    const std::shared_ptr<const ColorSpace>& dst) {
  ColorXformSteps steps{};
  // Null spaces mean the sRGB working space; dereference through the
  // interned sRGB singleton so the folding below always sees concrete
  // parameters.
  const ColorSpace& from = src ? *src : *ColorSpace::SRGB();
  const ColorSpace& to = dst ? *dst : *ColorSpace::SRGB();
  if (from == to) {
    // Same space: keep the no-op bit exact instead of running decode/encode
    // through the curve and its closed-form inverse.
    return steps;
  }

  if (!from.IsLinear()) {
    steps.decode = true;
    std::memcpy(steps.decode_fn, from.transfer_fn, sizeof(steps.decode_fn));
  }

  if (std::memcmp(from.gamut_xyz, to.gamut_xyz, sizeof(float) * 9) != 0) {
    float inv_dst[9];
    // A singular destination gamut keeps ColorSpace::Convert's lenient
    // behavior: the matrix stage is skipped rather than failing.
    if (ColorSpaceInvert3x3(to.gamut_xyz, inv_dst)) {
      steps.gamut = true;
      ColorSpaceMultiply3x3(inv_dst, from.gamut_xyz, steps.gamut_matrix);
    }
  }

  if (!to.IsLinear()) {
    steps.encode = true;
    ColorSpaceInvertTransferFn(to.transfer_fn, steps.encode_fn);
    steps.encode_is_srgb_curve =
        std::memcmp(to.transfer_fn, ColorSpace::SRGB()->transfer_fn,
                    sizeof(steps.encode_fn)) == 0;
  }

  return steps;
}

Color4f ColorXformSteps::Apply(Color4f color) const {
  float rgb[3] = {color.r, color.g, color.b};

  if (decode) {
    for (int i = 0; i < 3; i++) {
      rgb[i] = ColorSpaceEvalTransferFn(decode_fn, rgb[i]);
    }
  }

  if (gamut) {
    float out[3] = {rgb[0], rgb[1], rgb[2]};
    for (int row = 0; row < 3; row++) {
      rgb[row] = gamut_matrix[row * 3] * out[0] +
                 gamut_matrix[row * 3 + 1] * out[1] +
                 gamut_matrix[row * 3 + 2] * out[2];
    }
  }

  if (encode) {
    for (int i = 0; i < 3; i++) {
      rgb[i] = ColorSpaceEvalTransferFn(encode_fn, rgb[i]);
    }
  }

  return Color4f{rgb[0], rgb[1], rgb[2], color.a};
}

void ColorXformSteps::ApplyColors(Color4f colors[], int count) const {
  for (int i = 0; i < count; i++) {
    colors[i] = Apply(colors[i]);
  }
}

bool TransformPixels(const Pixmap& src, Pixmap* dst,
                     const ColorXformSteps& steps) {
  if (src.Width() == 0 || src.Height() == 0 || src.Width() != dst->Width() ||
      src.Height() != dst->Height()) {
    return false;
  }

  ColorType src_type = src.GetColorType();
  ColorType dst_type = dst->GetColorType();

  if (src_type == ColorType::kA8 && dst_type == ColorType::kA8) {
    // Alpha carries no color; copy the rows through unchanged.
    for (uint32_t y = 0; y < src.Height(); y++) {
      std::memcpy(
          static_cast<uint8_t*>(dst->WritableAddr()) + y * dst->RowBytes(),
          static_cast<const uint8_t*>(src.Addr()) + y * src.RowBytes(),
          src.Width());
    }
    return true;
  }

  if ((src_type != ColorType::kRGBA && src_type != ColorType::kBGRA) ||
      (dst_type != ColorType::kRGBA && dst_type != ColorType::kBGRA)) {
    // 16-bit layouts are not color managed yet.
    return false;
  }

  bool src_premul = src.GetAlphaType() == AlphaType::kPremul_AlphaType;
  bool dst_premul = dst->GetAlphaType() == AlphaType::kPremul_AlphaType;
  // A no-op transform must not pay for an unpremul -> premul round trip:
  // the divide/multiply pair is not byte exact.
  bool need_unpremul = src_premul && (!dst_premul || !steps.IsNoop());
  bool need_premul = dst_premul && (!src_premul || !steps.IsNoop());

  int src_r = src_type == ColorType::kRGBA ? 0 : 2;
  int src_b = src_type == ColorType::kRGBA ? 2 : 0;
  int dst_r = dst_type == ColorType::kRGBA ? 0 : 2;
  int dst_b = dst_type == ColorType::kRGBA ? 2 : 0;

  for (uint32_t y = 0; y < src.Height(); y++) {
    const uint8_t* src_row =
        static_cast<const uint8_t*>(src.Addr()) + y * src.RowBytes();
    uint8_t* dst_row =
        static_cast<uint8_t*>(dst->WritableAddr()) + y * dst->RowBytes();
    for (uint32_t x = 0; x < src.Width(); x++) {
      const uint8_t* s = src_row + x * 4;
      uint8_t* d = dst_row + x * 4;

      float a = s[3] / 255.f;
      float rgb[3] = {s[src_r] / 255.f, s[1] / 255.f, s[src_b] / 255.f};

      if (need_unpremul && a > 0.f) {
        rgb[0] /= a;
        rgb[1] /= a;
        rgb[2] /= a;
      }

      Color4f out = steps.Apply(Color4f{rgb[0], rgb[1], rgb[2], a});

      if (need_premul && out.a > 0.f) {
        rgb[0] = out.r * out.a;
        rgb[1] = out.g * out.a;
        rgb[2] = out.b * out.a;
      } else {
        rgb[0] = out.r;
        rgb[1] = out.g;
        rgb[2] = out.b;
      }

      d[dst_r] = Quantize8(rgb[0]);
      d[1] = Quantize8(rgb[1]);
      d[dst_b] = Quantize8(rgb[2]);
      d[3] = Quantize8(out.a);
    }
  }

  return true;
}

}  // namespace skity
