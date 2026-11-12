// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_COLOR_SPACE_PRIV_HPP
#define SRC_GRAPHIC_COLOR_SPACE_PRIV_HPP

#include <skity/graphic/color_space.hpp>

namespace skity {

// Internal color space math shared by ColorSpace::Convert, the CPU bitmap
// sampler and the GPU texture decode path (see src/graphic/color_space.cc).
// All matrices are row-major 3x3 RGB->XYZ gamut matrices unless stated
// otherwise.

// out = lhs * rhs
void ColorSpaceMultiply3x3(const float lhs[9], const float rhs[9],
                           float out[9]);

// Adjugate / determinant inversion; returns false when m is singular.
bool ColorSpaceInvert3x3(const float m[9], float inv[9]);

// Per-pair conversion invariants: the decode curve, the combined gamut
// matrix and the encode curve. Converting many colors prepares this once
// instead of redoing the matrix inversion and curve flip per pixel; the
// pointers reference the caller's spaces and must outlive the value.
struct PreparedConversion {
  const ColorSpace* decode_from = nullptr;
  float gamut[9] = {};
  bool needs_gamut = false;
  float encode_tf[7] = {};
  const ColorSpace* encode_to = nullptr;
};

PreparedConversion PrepareConversion(const ColorSpace& from,
                                     const ColorSpace& to);

// Applies a prepared conversion to one rgb triple in place. Extended-range
// inputs/outputs are preserved; clamping belongs to the quantizing caller.
void ApplyConversion(const PreparedConversion& prepared, float rgb[3]);

}  // namespace skity

#endif  // SRC_GRAPHIC_COLOR_SPACE_PRIV_HPP
