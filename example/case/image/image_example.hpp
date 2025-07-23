// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef EXAMPLE_CASE_IMAGE_IMAGE_EXAMPLE_HPP
#define EXAMPLE_CASE_IMAGE_IMAGE_EXAMPLE_HPP

#include <skity/gpu/gpu_context.hpp>
#include <skity/skity.hpp>

namespace skity::example::image {

std::shared_ptr<skity::Pixmap> load_bitmap(const char* path);

void draw_images(skity::Canvas* canvas, skity::GPUContext* gpu_context);

}  // namespace skity::example::image

#endif  // EXAMPLE_CASE_IMAGE_IMAGE_EXAMPLE_HPP
