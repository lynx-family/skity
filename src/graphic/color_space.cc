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

// CIE xy chromaticity (x, y) to XYZ with Y scaled to 1.
void XYZFromXY(float x, float y, float* xyz) {
  xyz[0] = x / y;
  xyz[1] = 1.f;
  xyz[2] = (1.f - x - y) / y;
}

// Bradford cone response matrix (row-major): the standard basis for
// chromatic adaptation between reference whites.
constexpr float kBradford[9] = {
    0.8951f,  0.2664f,  -0.1614f,  //
    -0.7502f, 1.7135f,  0.0367f,   //
    0.0389f,  -0.0685f, 1.0296f,
};

// The inverse cone matrix, as published: kBradford * kInverseBradford is
// the identity within 1e-7, the same an exact inversion would give.
constexpr float kInverseBradford[9] = {
    0.9869929f,  -0.1470543f, 0.1599627f,  //
    0.4323053f,  0.5183603f,  0.0492912f,  //
    -0.0085287f, 0.0400428f,  0.9684867f,
};

// Multiplies a row-major 3x3 matrix with a column vector.
void Multiply3x3Vector(const float matrix[9], const float vector[3],
                       float result[3]) {
  for (int row = 0; row < 3; row++) {
    const float* m = matrix + row * 3;
    result[row] = m[0] * vector[0] + m[1] * vector[1] + m[2] * vector[2];
  }
}

// The two ends of the white point bridge between skity and the outside
// world. Inside, every gamut matrix is stored and composed in the D50
// connection space (see ColorSpace::gamut_xyz): conversions between two
// stored matrices never touch a white point because both adaptations to D50
// cancel out. The whites only appear when translating at the boundary:
//
// - incoming (GamutFromChromaticities): Bradford(native white -> D50),
//   whatever white point the external description carried;
// - outgoing (ChromaticitiesFromGamut): Bradford(stored white -> D65), so
//   the exported chromaticities are stated in the D65-referenced form
//   nearly every display space publishes.

// CIE illuminant D50 as XYZ with Y == 1: the reference white every skity
// gamut matrix is adapted to (see ColorSpace::gamut_xyz).
constexpr float kD50XYZ[3] = {0.96422f, 1.f, 0.82521f};

// D65 chromaticity: the native white point of sRGB, Display P3 and nearly
// every other RGB display space; the reference white of the chromaticities
// ChromaticitiesFromGamut exports. A space whose native white is not D65 is
// normalized to a D65-referenced equivalent (exact for D65-native spaces,
// a Bradford approximation otherwise).
constexpr float kD65X = 0.3127f;
constexpr float kD65Y = 0.3290f;

// Builds the Bradford chromatic adaptation matrix mapping XYZ values
// relative to src_white onto XYZ values relative to dst_white:
//   adaptation = bradford^-1 * diag(dst_lms / src_lms) * bradford
// Returns false for a degenerate white point.
bool BradfordAdaptation(const float src_white[3], const float dst_white[3],
                        float adaptation[9]) {
  // XYZ -> LMS cone responses for both white points.
  float src_lms[3];
  float dst_lms[3];
  Multiply3x3Vector(kBradford, src_white, src_lms);
  Multiply3x3Vector(kBradford, dst_white, dst_lms);
  for (int i = 0; i < 3; i++) {
    // A physical white point has positive cone responses; anything else
    // (zero, negative, non-finite) makes the adaptation degenerate.
    if (!(src_lms[i] > 0.f) || !(dst_lms[i] > 0.f) ||
        !std::isfinite(src_lms[i]) || !std::isfinite(dst_lms[i])) {
      return false;
    }
  }

  // Scale each LMS axis by the destination-to-source white ratio.
  const float scale[9] = {
      dst_lms[0] / src_lms[0],
      0.f,
      0.f,  //
      0.f,
      dst_lms[1] / src_lms[1],
      0.f,  //
      0.f,
      0.f,
      dst_lms[2] / src_lms[2],
  };

  // LMS -> scale -> XYZ: bradford^-1 * scale * bradford.
  float scaled_bradford[9];
  ColorSpaceMultiply3x3(scale, kBradford, scaled_bradford);
  ColorSpaceMultiply3x3(kInverseBradford, scaled_bradford, adaptation);
  return true;
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

std::shared_ptr<const ColorSpace> ColorSpace::SRGB() {
  // Interned singleton: pointer comparison is a valid fast path for the
  // working-space default, and repeated conversions never re-allocate.
  // The const pointee keeps the shared object immutable for every holder.
  static const std::shared_ptr<const ColorSpace> kSRGB =
      std::make_shared<ColorSpace>();
  return kSRGB;
}

std::shared_ptr<const ColorSpace> ColorSpace::SRGBLinear() {
  static const std::shared_ptr<const ColorSpace> kSRGBLinear = [] {
    auto cs = std::make_shared<ColorSpace>();
    std::memcpy(cs->transfer_fn, kLinearTransferFn, sizeof(cs->transfer_fn));
    cs->kind = Kind::kSRGBLinear;
    return cs;
  }();
  return kSRGBLinear;
}

std::shared_ptr<const ColorSpace> ColorSpace::DisplayP3() {
  static const std::shared_ptr<const ColorSpace> kDisplayP3 = [] {
    auto cs = std::make_shared<ColorSpace>();
    std::memcpy(cs->gamut_xyz, kDisplayP3Gamut, sizeof(cs->gamut_xyz));
    cs->kind = Kind::kDisplayP3;
    return cs;
  }();
  return kDisplayP3;
}

std::shared_ptr<const ColorSpace> ColorSpace::MakeRGB(
    const float transfer_fn[7], const float gamut_xyz[9]) {
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

bool ColorSpace::GamutFromChromaticities(const float chromaticities[8],
                                         float gamut_xyz[9]) {
  const float* c = chromaticities;
  for (int i = 0; i < 8; i++) {
    // Values are ratios; x + y must stay below 1 for a physical color.
    if (!(c[i] > 0.f) || c[i] >= 1.f || !std::isfinite(c[i])) {
      return false;
    }
  }
  for (int i = 0; i < 4; i++) {
    // x + y == 1 is legal (z == 0, e.g. the Display P3 red primary); only
    // a negative z component makes the chromaticity unphysical. The
    // epsilon mirrors ChromaticitiesFromGamut, so boundary primaries
    // extracted as floats round trip despite x + y landing a few ulps
    // above 1.
    if (c[i * 2] + c[i * 2 + 1] > 1.f + 1e-4f) {
      return false;
    }
  }

  // Row-major primaries matrix (rows X/Y/Z, columns R/G/B, each column with
  // Y == 1) and the white point XYZ.
  float red[3], green[3], blue[3], white[3];
  XYZFromXY(c[2], c[3], red);
  XYZFromXY(c[4], c[5], green);
  XYZFromXY(c[6], c[7], blue);
  XYZFromXY(c[0], c[1], white);

  float primaries[9] = {
      red[0], green[0], blue[0],  //
      red[1], green[1], blue[1],  //
      red[2], green[2], blue[2],
  };

  // Scale each primary column so that RGB (1, 1, 1) maps to the white
  // point: scale = primaries^-1 * white.
  float inv[9];
  if (!ColorSpaceInvert3x3(primaries, inv)) {
    return false;
  }
  float scale[3];
  for (int row = 0; row < 3; row++) {
    scale[row] = inv[row * 3] * white[0] + inv[row * 3 + 1] * white[1] +
                 inv[row * 3 + 2] * white[2];
  }
  if (!(scale[0] > 0.f) || !(scale[1] > 0.f) || !(scale[2] > 0.f)) {
    return false;
  }

  // Native gamut: RGB -> XYZ relative to the space's own white point.
  float native[9];
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      native[row * 3 + col] = primaries[row * 3 + col] * scale[col];
    }
  }

  // Every gamut is stored adapted to the D50 connection space (see
  // gamut_xyz), so the native white point is adapted here. Skipping this
  // would combine e.g. a D65-referenced space with the D50-adapted
  // built-ins and tint every conversion.
  float adaptation[9];
  if (!BradfordAdaptation(white, kD50XYZ, adaptation)) {
    return false;
  }
  ColorSpaceMultiply3x3(adaptation, native, gamut_xyz);
  for (int i = 0; i < 9; i++) {
    if (!std::isfinite(gamut_xyz[i])) {
      return false;
    }
  }
  return true;
}

bool ColorSpace::ChromaticitiesFromGamut(const float gamut_xyz[9],
                                         float chromaticities[8]) {
  // The white of a gamut matrix is where it maps RGB (1, 1, 1): its row
  // sums, which are D50 by contract.
  const float* m = gamut_xyz;
  float white[3] = {m[0] + m[1] + m[2], m[3] + m[4] + m[5], m[6] + m[7] + m[8]};

  // Undo the D50 adaptation towards D65, so the values match the published
  // native primaries. Adapting D65 back to D50 lands on the same matrix
  // again, whatever white point the description originally had.
  float d65[3];
  XYZFromXY(kD65X, kD65Y, d65);
  float adaptation[9];
  if (!BradfordAdaptation(white, d65, adaptation)) {
    return false;
  }
  float native[9];
  ColorSpaceMultiply3x3(adaptation, gamut_xyz, native);

  // xy of an XYZ triple; rejects anything GamutFromChromaticities would
  // reject, so extracted values always parse back. The epsilon admits
  // primaries sitting on the x + y == 1 boundary (z == 0, like the Display
  // P3 red), whose un-adapted z is only zero up to float noise; the chunk
  // values themselves quantize to exactly 1.
  auto xy_of = [](float x, float y, float z, float* out) {
    float sum = x + y + z;
    if (!(sum > 0.f) || !std::isfinite(sum)) {
      return false;
    }
    out[0] = x / sum;
    out[1] = y / sum;
    return out[0] > 0.f && out[0] < 1.f && out[1] > 0.f && out[1] < 1.f &&
           out[0] + out[1] <= 1.f + 1e-4f;
  };
  const float* n = native;
  return xy_of(n[0] + n[1] + n[2], n[3] + n[4] + n[5], n[6] + n[7] + n[8],
               chromaticities) &&
         xy_of(n[0], n[3], n[6], chromaticities + 2) &&
         xy_of(n[1], n[4], n[7], chromaticities + 4) &&
         xy_of(n[2], n[5], n[8], chromaticities + 6);
}

bool ColorSpace::IsSRGB() const {
  const ColorSpace srgb{};
  return SameTransferFn(transfer_fn, srgb.transfer_fn) &&
         SameGamut(gamut_xyz, srgb.gamut_xyz);
}

bool ColorSpace::IsLinear() const {
  // Identity power segment (g == 1, a == 1, b == 0, e == 0) plus a linear
  // segment that is identity (c == 1, f == 0) wherever it is reachable:
  // with d > 0 the segment covers [0, d), with d == 0 only negative inputs
  // reach it, where c == 0 mirrors the (identity) power branch. A nonzero c
  // with d == 0 scales negatives and is not the identity.
  return transfer_fn[0] == 1.f && transfer_fn[1] == 1.f &&
         transfer_fn[2] == 0.f && transfer_fn[5] == 0.f &&
         (transfer_fn[3] == 1.f || transfer_fn[3] == 0.f) &&
         transfer_fn[6] == 0.f;
}

bool ColorSpace::operator==(const ColorSpace& other) const {
  return SameTransferFn(transfer_fn, other.transfer_fn) &&
         SameGamut(gamut_xyz, other.gamut_xyz);
}

namespace {

// Null spaces mean the sRGB working space; dereference through the
// default-constructed sRGB value (the same bytes the SRGB() singleton and
// every default ColorSpace hold) so the math below always sees concrete
// parameters.
const ColorSpace& DerefOrSRGB(
    const std::shared_ptr<const ColorSpace>& color_space) {
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

Color4f ColorSpace::Convert(const std::shared_ptr<const ColorSpace>& src,
                            Color4f color,
                            const std::shared_ptr<const ColorSpace>& dst) {
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

void ColorSpace::ConvertColors(const std::shared_ptr<const ColorSpace>& src,
                               Color4f colors[], int count,
                               const std::shared_ptr<const ColorSpace>& dst) {
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
