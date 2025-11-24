// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_EFFECT_GRADIENT_FALLBACK_HPP
#define SRC_EFFECT_GRADIENT_FALLBACK_HPP

#include <skity/effect/shader.hpp>
#include <skity/graphic/color.hpp>

namespace skity {

bool NeedsFallbackToSolidColor(Shader::GradientType type,
                               const Shader::GradientInfo& info,
                               Color4f& fallback_color);

}  // namespace skity

#endif  // SRC_EFFECT_GRADIENT_FALLBACK_HPP
