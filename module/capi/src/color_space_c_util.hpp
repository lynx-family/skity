// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_SRC_COLOR_SPACE_C_UTIL_HPP
#define MODULE_CAPI_SRC_COLOR_SPACE_C_UTIL_HPP

#include <skity/c/skity_types.h>

#include <skity/graphic/color_space.hpp>

namespace skity::capi {

// Converts a non-null skity_color_space_info descriptor into a core color
// space. Named kinds map to the interned singletons; the parametric kind
// goes through MakeRGB, falling back to sRGB when the parameters cannot
// describe a valid space. Callers decide what a NULL descriptor means
// (undeclared / sRGB) before calling.
inline std::shared_ptr<const ColorSpace> ColorSpaceFromCInfo(
    const skity_color_space_info* info) {
  switch (info->kind) {
    case SKITY_COLOR_SPACE_SRGB:
      return ColorSpace::SRGB();
    case SKITY_COLOR_SPACE_SRGB_LINEAR:
      return ColorSpace::SRGBLinear();
    case SKITY_COLOR_SPACE_DISPLAY_P3:
      return ColorSpace::DisplayP3();
    case SKITY_COLOR_SPACE_ADOBE_RGB:
      return ColorSpace::AdobeRGB();
    case SKITY_COLOR_SPACE_PARAMETRIC:
      break;
  }
  auto parametric = ColorSpace::MakeRGB(info->transfer_fn, info->gamut_xyz);
  return parametric ? parametric : ColorSpace::SRGB();
}

}  // namespace skity::capi

#endif  // MODULE_CAPI_SRC_COLOR_SPACE_C_UTIL_HPP
