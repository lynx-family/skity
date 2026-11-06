// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_COLOR_SPACE_PRIV_HPP
#define SRC_GRAPHIC_COLOR_SPACE_PRIV_HPP

namespace skity {

// Internal color space math shared by ColorSpace::Convert and the GPU texture
// decode path (see src/graphic/color_space.cc). All matrices are row-major
// 3x3 RGB->XYZ gamut matrices unless stated otherwise.

// out = lhs * rhs
void ColorSpaceMultiply3x3(const float lhs[9], const float rhs[9],
                           float out[9]);

// Adjugate / determinant inversion; returns false when m is singular.
bool ColorSpaceInvert3x3(const float m[9], float inv[9]);

}  // namespace skity

#endif  // SRC_GRAPHIC_COLOR_SPACE_PRIV_HPP
