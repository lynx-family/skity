// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
#include <cstring>
#include <memory>
#include <skity/graphic/color_space.hpp>

#include "src/graphic/color_space_priv.hpp"

namespace skity {

namespace {

constexpr float kLinearTransferFn[7] = {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};

// Display P3 gamut in the D50 connection space (see ColorSpace::gamut_xyz).
// Derived from the standard primaries R(0.680, 0.320) G(0.265, 0.690)
// B(0.150, 0.060) under the P3 native white D65 (0.3127, 0.3290): the
// native RGB->XYZ matrix (white scaled into the columns) is
// Bradford-adapted from D65 to CIE D50 XYZ (0.96422, 1, 0.82521), so the
// rows sum to that D50 white exactly as the sRGB constants above do. The
// unit tests re-derive this matrix independently from the primaries and
// verify the adaptation cancels on neutral grays.
constexpr float kDisplayP3Gamut[9] = {
    0.5151021f,  0.2919648f, 0.1571531f,  //
    0.2411818f,  0.6922363f, 0.0665819f,  //
    -0.0010494f, 0.0418818f, 0.7843776f,
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

// Evaluates the parametric curve. The power branch clamps its base instead of
// the input so out-of-gamut (negative) linear values encode to slightly
// negative signals rather than NaN; the linear branch extrapolates naturally.
// A degenerate linear segment (pure power curve, c == f == 0) would collapse
// negative inputs to zero; those mirror the power branch instead so
// extended-range colors keep their sign and the curve stays invertible.
float EvalTransferFn(const float tf[7], float x) {
  if (x < tf[4]) {
    if (tf[3] == 0.f && tf[6] == 0.f) {
      return -std::pow(-x, tf[0]);
    }
    return tf[3] * x + tf[6];
  }
  float base = std::fmax(tf[1] * x + tf[2], 0.f);
  return std::pow(base, tf[0]) + tf[5];
}

// The parametric family is closed under inversion (same closed-form flip
// skcms uses): the result evaluates the linear -> signal direction.
void InvertTransferFn(const float tf[7], float inv[7]) {
  const float inv_a_pow_g = std::pow(tf[1], -tf[0]);
  inv[0] = 1.f / tf[0];
  inv[1] = inv_a_pow_g;
  inv[2] = -tf[5] * inv_a_pow_g;
  inv[3] = tf[3] > 0.f ? 1.f / tf[3] : 0.f;
  inv[4] = tf[3] * tf[4] + tf[6];
  inv[5] = -tf[2] / tf[1];
  inv[6] = tf[3] > 0.f ? -tf[6] / tf[3] : 0.f;
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

std::shared_ptr<ColorSpace> ColorSpace::SRGB() {
  // Interned singleton: pointer comparison is a valid fast path for the
  // working-space default, and repeated conversions never re-allocate.
  static const std::shared_ptr<ColorSpace> kSRGB =
      std::make_shared<ColorSpace>();
  return kSRGB;
}

std::shared_ptr<ColorSpace> ColorSpace::SRGBLinear() {
  static const std::shared_ptr<ColorSpace> kSRGBLinear = [] {
    auto cs = std::make_shared<ColorSpace>();
    std::memcpy(cs->transfer_fn, kLinearTransferFn, sizeof(cs->transfer_fn));
    cs->kind = Kind::kSRGBLinear;
    return cs;
  }();
  return kSRGBLinear;
}

std::shared_ptr<ColorSpace> ColorSpace::DisplayP3() {
  static const std::shared_ptr<ColorSpace> kDisplayP3 = [] {
    auto cs = std::make_shared<ColorSpace>();
    std::memcpy(cs->gamut_xyz, kDisplayP3Gamut, sizeof(cs->gamut_xyz));
    cs->kind = Kind::kDisplayP3;
    return cs;
  }();
  return kDisplayP3;
}

std::shared_ptr<ColorSpace> ColorSpace::MakeRGB(const float transfer_fn[7],
                                                const float gamut_xyz[9]) {
  if (transfer_fn == nullptr || gamut_xyz == nullptr) {
    return nullptr;
  }
  if (!IsValidTransferFn(transfer_fn) || !IsValidGamut(gamut_xyz)) {
    return nullptr;
  }
  auto cs = std::make_shared<ColorSpace>();
  std::memcpy(cs->transfer_fn, transfer_fn, sizeof(cs->transfer_fn));
  std::memcpy(cs->gamut_xyz, gamut_xyz, sizeof(cs->gamut_xyz));
  cs->kind = Kind::kParametric;
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

namespace {

// Null spaces mean the sRGB working space; dereference through the interned
// sRGB bytes so the math below always sees concrete parameters.
const ColorSpace& DerefOrSRGB(const std::shared_ptr<ColorSpace>& color_space) {
  static const ColorSpace kSRGBInstance{};
  return color_space ? *color_space : kSRGBInstance;
}

// The conversion for one space pair with every per-pixel invariant folded
// up front: the decode curve, the combined gamut matrix and the encode
// curve. Converting a batch prepares this once instead of redoing the
// matrix inversion and curve flip per pixel.
struct PreparedConversion {
  const ColorSpace* decode_from = nullptr;
  float gamut[9] = {};
  bool needs_gamut = false;
  float encode_tf[7] = {};
  const ColorSpace* encode_to = nullptr;
};

PreparedConversion PrepareConversion(const ColorSpace& from,
                                     const ColorSpace& to) {
  PreparedConversion prepared;
  if (!from.IsLinear()) {
    prepared.decode_from = &from;
  }
  if (!SameGamut(from.gamut_xyz, to.gamut_xyz)) {
    float inv_dst[9];
    if (ColorSpaceInvert3x3(to.gamut_xyz, inv_dst)) {
      ColorSpaceMultiply3x3(inv_dst, from.gamut_xyz, prepared.gamut);
      prepared.needs_gamut = true;
    }
  }
  if (!to.IsLinear()) {
    InvertTransferFn(to.transfer_fn, prepared.encode_tf);
    prepared.encode_to = &to;
  }
  return prepared;
}

void ApplyConversion(const PreparedConversion& prepared, float rgb[3]) {
  if (prepared.decode_from != nullptr) {
    for (int i = 0; i < 3; i++) {
      rgb[i] = EvalTransferFn(prepared.decode_from->transfer_fn, rgb[i]);
    }
  }
  if (prepared.needs_gamut) {
    float out[3] = {rgb[0], rgb[1], rgb[2]};
    for (int row = 0; row < 3; row++) {
      rgb[row] = prepared.gamut[row * 3] * out[0] +
                 prepared.gamut[row * 3 + 1] * out[1] +
                 prepared.gamut[row * 3 + 2] * out[2];
    }
  }
  if (prepared.encode_to != nullptr) {
    for (int i = 0; i < 3; i++) {
      rgb[i] = EvalTransferFn(prepared.encode_tf, rgb[i]);
    }
  }
}

}  // namespace

Color4f ColorSpace::Convert(std::shared_ptr<ColorSpace> src, Color4f color,
                            std::shared_ptr<ColorSpace> dst) {
  if (ColorSpaceEquals(src, dst)) {
    return color;
  }

  const ColorSpace& from = DerefOrSRGB(src);
  const ColorSpace& to = DerefOrSRGB(dst);
  PreparedConversion prepared = PrepareConversion(from, to);

  float rgb[3] = {color.r, color.g, color.b};
  ApplyConversion(prepared, rgb);
  return Color4f{rgb[0], rgb[1], rgb[2], color.a};
}

void ColorSpace::ConvertColors(std::shared_ptr<ColorSpace> src,
                               Color4f colors[], int count,
                               std::shared_ptr<ColorSpace> dst) {
  if (ColorSpaceEquals(src, dst)) {
    return;
  }
  // The per-pair invariants are prepared once for the whole batch; only
  // the per-color math runs inside the loop.
  PreparedConversion prepared =
      PrepareConversion(DerefOrSRGB(src), DerefOrSRGB(dst));
  for (int i = 0; i < count; i++) {
    float rgb[3] = {colors[i].r, colors[i].g, colors[i].b};
    ApplyConversion(prepared, rgb);
    colors[i] = Color4f{rgb[0], rgb[1], rgb[2], colors[i].a};
  }
}

}  // namespace skity
