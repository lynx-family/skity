/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/text/font_style.hpp>

#include "src/geometry/math.hpp"

namespace skity {

static constexpr float axis_width_value[9]{1, 2, 3, 4, 5, 6, 7, 8, 9};
static constexpr float axis_width_key[0x10] = {50,    50,  62.5, 75,  87.5, 100,
                                               112.5, 125, 150,  200, 200,  200,
                                               200,   200, 200,  200};

template <typename T>
static constexpr const T& clamp(const T& x, const T& lo, const T& hi) {
  return std::max(lo, std::min(x, hi));
}

FontStyle::FontStyle(int weight, int width, Slant slant)
    : value_((clamp<int>(weight, kInvisible_Weight, kExtraBlack_Weight)) +
             (clamp<int>(width, kUltraCondensed_Width, kUltraExpanded_Width)
              << 16) +
             (clamp<int>(slant, kUpright_Slant, kOblique_Slant) << 24)) {}

FontStyle::FontStyle()
    : FontStyle(kNormal_Weight, kNormal_Width, kUpright_Slant) {}

FontStyle::Width FontStyle::WidthFromAxisWidth(float axis_width) {
  int usWidth = RoundToInt(
      FloatInterpFunc(axis_width, &axis_width_key[1], axis_width_value, 9));
  return static_cast<FontStyle::Width>(usWidth);
}

}  // namespace skity
