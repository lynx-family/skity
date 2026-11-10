// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
#include <cstring>
#include <skity/graphic/color_space.hpp>

#include "src/graphic/color_space_priv.hpp"
#include "src/graphic/color_xform.hpp"

namespace skity {

namespace {

constexpr float kLinearTransferFn[7] = {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};

// Display P3 gamut (D65 white), derived from the standard primaries
// R(0.680, 0.320) G(0.265, 0.695) B(0.150, 0.060): each column scales the
// primary's XYZ, so the rows sum to the D65 white point (0.95046, 1, 1.08906).
constexpr float kDisplayP3Gamut[9] = {
    0.4878489f, 0.2633719f, 0.1992349f,  //
    0.2295760f, 0.6907301f, 0.0796940f,  //
    0.f,        0.0397542f, 1.0493037f,
};

// Adobe RGB (1998) gamut (D65 white), from the standard primaries
// R(0.640, 0.330) G(0.210, 0.710) B(0.150, 0.060); rows sum to D65 and the
// column ratios recover the primaries (see the unit test anchors).
constexpr float kAdobeRGBGamut[9] = {
    0.5766690f, 0.1855582f, 0.1882286f,  //
    0.2973450f, 0.6273636f, 0.0752915f,  //
    0.0270314f, 0.0706889f, 0.9913375f,
};

bool IsValidTransferFn(const float tf[7]) {
  for (int i = 0; i < 7; i++) {
    if (!std::isfinite(tf[i])) {
      return false;
    }
  }
  // The power segment must be a usable power curve and start inside [0, 1].
  if (!(tf[0] > 0.f) || !(tf[1] > 0.f) || tf[4] < 0.f || tf[4] > 1.f) {
    return false;
  }
  // The linear segment must be usable unless the power segment already covers
  // the whole domain (pure gamma curve, d == 0).
  if (tf[3] <= 0.f && tf[4] > 0.f) {
    return false;
  }
  return true;
}

bool IsValidGamut(const float m[9]) {
  float det = m[0] * (m[4] * m[8] - m[5] * m[7]) -
              m[1] * (m[3] * m[8] - m[5] * m[6]) +
              m[2] * (m[3] * m[7] - m[4] * m[6]);
  return std::isfinite(det) && det != 0.f;
}

bool SameTransferFn(const float lhs[7], const float rhs[7]) {
  return std::memcmp(lhs, rhs, sizeof(float) * 7) == 0;
}

bool SameGamut(const float lhs[9], const float rhs[9]) {
  return std::memcmp(lhs, rhs, sizeof(float) * 9) == 0;
}

}  // namespace

void ColorSpaceMultiply3x3(const float lhs[9], const float rhs[9],
                           float out[9]) {
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      out[row * 3 + col] = lhs[row * 3] * rhs[col] +
                           lhs[row * 3 + 1] * rhs[3 + col] +
                           lhs[row * 3 + 2] * rhs[6 + col];
    }
  }
}

bool ColorSpaceInvert3x3(const float m[9], float inv[9]) {
  float det = m[0] * (m[4] * m[8] - m[5] * m[7]) -
              m[1] * (m[3] * m[8] - m[5] * m[6]) +
              m[2] * (m[3] * m[7] - m[4] * m[6]);
  if (det == 0.f || !std::isfinite(det)) {
    return false;
  }
  float inv_det = 1.f / det;
  inv[0] = (m[4] * m[8] - m[5] * m[7]) * inv_det;
  inv[1] = (m[2] * m[7] - m[1] * m[8]) * inv_det;
  inv[2] = (m[1] * m[5] - m[2] * m[4]) * inv_det;
  inv[3] = (m[5] * m[6] - m[3] * m[8]) * inv_det;
  inv[4] = (m[0] * m[8] - m[2] * m[6]) * inv_det;
  inv[5] = (m[2] * m[3] - m[0] * m[5]) * inv_det;
  inv[6] = (m[3] * m[7] - m[4] * m[6]) * inv_det;
  inv[7] = (m[1] * m[6] - m[0] * m[7]) * inv_det;
  inv[8] = (m[0] * m[4] - m[1] * m[3]) * inv_det;
  return true;
}

float ColorSpaceEvalTransferFn(const float tf[7], float x) {
  if (x < tf[4]) {
    return tf[3] * x + tf[6];
  }
  float base = std::fmax(tf[1] * x + tf[2], 0.f);
  return std::pow(base, tf[0]) + tf[5];
}

void ColorSpaceInvertTransferFn(const float tf[7], float inv[7]) {
  const float inv_a_pow_g = std::pow(tf[1], -tf[0]);
  inv[0] = 1.f / tf[0];
  inv[1] = inv_a_pow_g;
  inv[2] = -tf[5] * inv_a_pow_g;
  inv[3] = tf[3] > 0.f ? 1.f / tf[3] : 0.f;
  inv[4] = tf[3] * tf[4] + tf[6];
  inv[5] = -tf[2] / tf[1];
  inv[6] = tf[3] > 0.f ? -tf[6] / tf[3] : 0.f;
}

ColorSpace ColorSpace::SRGBLinear() {
  ColorSpace cs{};
  std::memcpy(cs.transfer_fn, kLinearTransferFn, sizeof(cs.transfer_fn));
  cs.kind = Kind::kSRGBLinear;
  return cs;
}

ColorSpace ColorSpace::DisplayP3() {
  ColorSpace cs{};
  std::memcpy(cs.gamut_xyz, kDisplayP3Gamut, sizeof(cs.gamut_xyz));
  cs.kind = Kind::kDisplayP3;
  return cs;
}

ColorSpace ColorSpace::AdobeRGB() {
  ColorSpace cs{};
  // Pure gamma 2.2 power curve (single segment, d == 0).
  cs.transfer_fn[0] = 2.2f;
  cs.transfer_fn[1] = 1.f;
  cs.transfer_fn[2] = 0.f;
  cs.transfer_fn[3] = 0.f;
  cs.transfer_fn[4] = 0.f;
  cs.transfer_fn[5] = 0.f;
  cs.transfer_fn[6] = 0.f;
  std::memcpy(cs.gamut_xyz, kAdobeRGBGamut, sizeof(cs.gamut_xyz));
  cs.kind = Kind::kAdobeRGB;
  return cs;
}

std::optional<ColorSpace> ColorSpace::MakeRGB(const float transfer_fn[7],
                                              const float gamut_xyz[9]) {
  if (transfer_fn == nullptr || gamut_xyz == nullptr) {
    return std::nullopt;
  }
  if (!IsValidTransferFn(transfer_fn) || !IsValidGamut(gamut_xyz)) {
    return std::nullopt;
  }
  ColorSpace cs{};
  std::memcpy(cs.transfer_fn, transfer_fn, sizeof(cs.transfer_fn));
  std::memcpy(cs.gamut_xyz, gamut_xyz, sizeof(cs.gamut_xyz));
  cs.kind = Kind::kParametric;
  return cs;
}

bool ColorSpace::IsSRGB() const {
  const ColorSpace srgb{};
  return SameTransferFn(transfer_fn, srgb.transfer_fn) &&
         SameGamut(gamut_xyz, srgb.gamut_xyz);
}

bool ColorSpace::IsLinear() const {
  // Identity power segment (g == 1, a == 1, b == 0, e == 0) plus an identity
  // or unreachable linear segment.
  return transfer_fn[0] == 1.f && transfer_fn[1] == 1.f &&
         transfer_fn[2] == 0.f && transfer_fn[5] == 0.f &&
         (transfer_fn[4] == 0.f ||
          (transfer_fn[3] == 1.f && transfer_fn[6] == 0.f));
}

bool ColorSpace::operator==(const ColorSpace& other) const {
  return SameTransferFn(transfer_fn, other.transfer_fn) &&
         SameGamut(gamut_xyz, other.gamut_xyz);
}

Color4f ColorSpace::Convert(Color4f color, const ColorSpace& dst) const {
  // Delegates to the folded stage pipeline so every conversion boundary
  // (CPU, GPU uniform setup, buffer kernels) shares one implementation.
  return ColorXformSteps::Resolve(*this, dst).Apply(color);
}

void ColorSpace::ConvertColors(Color4f colors[], int count,
                               const ColorSpace& dst) const {
  ColorXformSteps::Resolve(*this, dst).ApplyColors(colors, count);
}

}  // namespace skity
