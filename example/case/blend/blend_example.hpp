// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef EXAMPLE_BLEND_BLEND_EXAMPLE_HPP
#define EXAMPLE_BLEND_BLEND_EXAMPLE_HPP

#include <skity/skity.hpp>

namespace skity::example::blend {

uint32_t get_blend_case_count();

const char* draw_blend_case(skity::Canvas* canvas, uint32_t index);

}  // namespace skity::example::blend

#endif  // EXAMPLE_BLEND_BLEND_EXAMPLE_HPP
