// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_COLOR_TYPE_HPP
#define INCLUDE_SKITY_GRAPHIC_COLOR_TYPE_HPP

namespace skity {

/**
 * @enum ColorType
 * Describes how pixel bits encode color.
 *
 * Currently only support RGBA or BGRA color type.
 */
enum class ColorType {
  kUnknown,  // uninitialized
  kRGBA,     // pixel with 8 bits for red, green, blue, alpha; in 32-bit word
  kBGRA,     // pixel with 8 bits for blue, green ,red, alpha; in 32-bit word
  kRGB565,   // pixel with 5 bits red, 6 bits green, 5 bits blue, in 16-bit word
  kA8,       // pixel with alpha in 8-bit byte, It should be noted that it is
             // currently only used internally in skity
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_COLOR_TYPE_HPP
