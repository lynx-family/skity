// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_BLEND_MODE_PRIV_HPP
#define SRC_GRAPHIC_BLEND_MODE_PRIV_HPP

#include <skity/graphic/blend_mode.hpp>
#include <skity/graphic/color.hpp>
#include <skity/macros.hpp>

#ifdef SKITY_ARM_NEON
#include <arm_neon.h>
#endif

namespace skity {

PMColor PorterDuffBlend(PMColor src, PMColor dst, BlendMode mode);

#ifdef SKITY_ARM_NEON
void ProterDuffBlendNeon(uint32_t* src, uint32_t* dst, uint32_t len,
                         BlendMode mode);

void ProterDuffBlendNeon(uint32_t src, uint32_t* dst, uint32_t len,
                         BlendMode mode);

#endif

}  // namespace skity
#endif  // SRC_GRAPHIC_BLEND_MODE_PRIV_HPP
