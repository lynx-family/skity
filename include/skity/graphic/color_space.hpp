// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_COLOR_SPACE_HPP
#define INCLUDE_SKITY_GRAPHIC_COLOR_SPACE_HPP

#include <cstdint>
#include <optional>
#include <skity/graphic/color.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * A descriptive color space value: a transfer function plus an RGB->XYZ gamut
 * matrix. The default-constructed ColorSpace is sRGB, which is also the
 * pipeline working space, so "no color space specified" and sRGB are the same
 * state and no allocation or null handling is needed.
 */
struct SKITY_API ColorSpace {
  enum class Kind : uint8_t {
    kSRGB,
    kSRGBLinear,
    kDisplayP3,
    kAdobeRGB,
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
   * Row-major RGB->XYZ (D65) gamut matrix. Defaults to the sRGB primaries,
   * kept as 16-bit fixed point constants so the color filter serialization
   * and this value stay bit-identical.
   */
  float gamut_xyz[9] = {
      0x6FA2 / 65536.f, 0x6299 / 65536.f, 0x24A0 / 65536.f,  //
      0x38F5 / 65536.f, 0xB785 / 65536.f, 0x0F84 / 65536.f,  //
      0x0390 / 65536.f, 0x18DA / 65536.f, 0xB6CF / 65536.f,
  };

  /// Records where the value came from; equality ignores it.
  Kind kind = Kind::kSRGB;

  /// The sRGB color space; identical to the default-constructed value.
  static ColorSpace SRGB() { return ColorSpace{}; }

  /// Linear transfer function with the sRGB gamut.
  static ColorSpace SRGBLinear();

  /// Display P3 color space (sRGB transfer curve, P3 D65 gamut).
  static ColorSpace DisplayP3();

  /**
   * Adobe RGB (1998): a pure gamma 2.2 transfer curve over the Adobe RGB
   * D65 gamut (wider than sRGB in greens/cyans).
   */
  static ColorSpace AdobeRGB();

  /**
   * Creates a parametric space from a skcms-style transfer function and a
   * row-major RGB->XYZ gamut matrix. Returns nullopt when the parameters
   * cannot describe a valid space.
   */
  static std::optional<ColorSpace> MakeRGB(const float transfer_fn[7],
                                           const float gamut_xyz[9]);

  /// Returns true if both the transfer function and the gamut are sRGB.
  bool IsSRGB() const;

  /// Returns true if the transfer function is the identity (linear).
  bool IsLinear() const;

  /// Numeric equality over the 16 descriptive floats.
  bool operator==(const ColorSpace& other) const;
  bool operator!=(const ColorSpace& other) const { return !(*this == other); }

  /**
   * Converts an unpremultiplied RGBA color from this color space to dst.
   * Alpha passes through untouched; out-of-gamut RGB values are not clamped
   * (quantization clamps). A default-constructed dst is the pipeline working
   * space (sRGB).
   */
  Color4f Convert(Color4f color, const ColorSpace& dst) const;

  /// Bulk in-place variant of Convert().
  void ConvertColors(Color4f colors[], int count, const ColorSpace& dst) const;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_COLOR_SPACE_HPP
