// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SKITY_EXAMPLE_COMMON_APP_UTILS_HPP
#define SKITY_EXAMPLE_COMMON_APP_UTILS_HPP

#include <skity/skity.hpp>

namespace skity {
namespace example {

void set_bitmap_color(skity::Bitmap& bitmap, const skity::Rect& bounds,
                      skity::Color color);

std::shared_ptr<skity::Pixmap> make_rect_image(
    float out_size, skity::Color out_color = skity::Color_TRANSPARENT,
    float in_size = -1.f, skity::Color in_color = skity::Color_TRANSPARENT,
    float in_offset = -1.f);

}  // namespace example
}  // namespace skity

#endif  // SKITY_EXAMPLE_COMMON_APP_UTILS_HPP
