// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_COLOR_SPACE_HPP
#define INCLUDE_SKITY_GRAPHIC_COLOR_SPACE_HPP

#include <cstdint>
#include <memory>
#include <skity/graphic/color.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * A descriptive color space: a transfer function plus an RGB->XYZ gamut
 * matrix. Spaces are passed around as std::shared_ptr<const ColorSpace>
 * (like the other reference-typed effects, but immutable: the named
 * factories return interned singletons shared by every holder, so nothing
 * may write through the pointer). A null pointer means "no color space
 * declared", which equals the sRGB pipeline working space, so entry points
 * accept nullptr as the default and treat it as sRGB. Pointer comparison is
 * a valid fast path for the singletons, but ColorSpaceEquals stays numeric
 * for deserialized spaces.
 */
struct SKITY_API ColorSpace {
  enum class Kind : uint8_t {
    kSRGB,
    kSRGBLinear,
    kDisplayP3,
    kParametric,
  };

  /**
   * skcms-style transfer function parameters {g, a, b, c, d, e, f} describing
   * the signal -> linear (linearization) curve:
   *   linear(x) = c * x + f               for x <  d
   *   linear(x) = pow(a * x + b, g) + e   for x >= d
   * Defaults to the sRGB curve.
   */
  float transfer_fn[7] = {
      2.4f,                              // g
      static_cast<float>(1. / 1.055),    // a
      static_cast<float>(.055 / 1.055),  // b
      static_cast<float>(1. / 12.92),    // c
      0.04045f,                          // d
      0.f,                               // e
      0.f,                               // f
  };

  /**
   * Row-major RGB->XYZ gamut matrix, Bradford-adapted to the D50 connection
   * space (CIE illuminant D50, XYZ = (0.96422, 1, 0.82521)) - the convention
   * ICC profiles use for their colorants. Every gamut in skity shares this
   * one reference white, whatever the space's native white point is, so a
   * src -> dst conversion composes as inverse(dst.gamut_xyz) * src.gamut_xyz
   * with the adaptations cancelling out. A matrix left in its native white
   * (e.g. D65) must not be stored here: mixing reference whites tints every
   * cross-gamut conversion, neutral grays included.
   *
   * Defaults to the sRGB primaries as adapted by the standard ICC sRGB
   * profile, kept as 16-bit fixed point constants so the color filter
   * serialization and this value stay bit-identical.
   */
  float gamut_xyz[9] = {
      0x6FA2 / 65536.f, 0x6299 / 65536.f, 0x24A0 / 65536.f,  //
      0x38F5 / 65536.f, 0xB785 / 65536.f, 0x0F84 / 65536.f,  //
      0x0390 / 65536.f, 0x18DA / 65536.f, 0xB6CF / 65536.f,
  };

  /// Records where the value came from; equality ignores it.
  Kind kind = Kind::kSRGB;

  /// The sRGB color space; identical bytes to the default-constructed value.
  static std::shared_ptr<const ColorSpace> SRGB();

  /// Linear transfer function with the sRGB gamut.
  static std::shared_ptr<const ColorSpace> SRGBLinear();

  /// Display P3 color space (sRGB transfer curve, P3 primaries with a D65
  /// native white, stored D50-adapted like every gamut).
  static std::shared_ptr<const ColorSpace> DisplayP3();

  /**
   * Creates a parametric space from a skcms-style transfer function and a
   * row-major RGB->XYZ gamut matrix. The matrix must already be adapted to
   * the D50 connection space (see gamut_xyz); primaries given in their
   * native white point need a Bradford adaptation first. Returns nullptr
   * when the parameters cannot describe a valid space.
   */
  static std::shared_ptr<const ColorSpace> MakeRGB(const float transfer_fn[7],
                                                   const float gamut_xyz[9]);

  /// Returns true if both the transfer function and the gamut are sRGB.
  bool IsSRGB() const;

  /// Returns true if the transfer function is the identity (linear).
  bool IsLinear() const;

  /// Numeric equality over the 16 descriptive floats.
  bool operator==(const ColorSpace& other) const;
  bool operator!=(const ColorSpace& other) const { return !(*this == other); }

  /**
   * Converts an unpremultiplied RGBA color from src to dst. Null spaces mean
   * sRGB (the pipeline working space). Alpha passes through untouched;
   * out-of-gamut RGB values are not clamped (quantization clamps).
   */
  static Color4f Convert(const std::shared_ptr<const ColorSpace>& src,
                         Color4f color,
                         const std::shared_ptr<const ColorSpace>& dst);

  /// Bulk in-place variant of Convert().
  static void ConvertColors(const std::shared_ptr<const ColorSpace>& src,
                            Color4f colors[], int count,
                            const std::shared_ptr<const ColorSpace>& dst);
};

/// Null-tolerant sRGB check: an undeclared space (nullptr) is sRGB.
inline bool ColorSpaceIsSRGB(
    const std::shared_ptr<const ColorSpace>& color_space) {
  return !color_space || color_space->IsSRGB();
}

/// Null-tolerant equality: nullptr compares equal to sRGB, otherwise numeric.
inline bool ColorSpaceEquals(const std::shared_ptr<const ColorSpace>& lhs,
                             const std::shared_ptr<const ColorSpace>& rhs) {
  if (lhs == rhs) {
    return true;
  }
  if (!lhs || !rhs) {
    return lhs ? lhs->IsSRGB() : rhs->IsSRGB();
  }
  return *lhs == *rhs;
}

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_COLOR_SPACE_HPP
