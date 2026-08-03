// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef EXAMPLE_CASE_BICUBIC_BICUBIC_EXAMPLE_HPP
#define EXAMPLE_CASE_BICUBIC_BICUBIC_EXAMPLE_HPP

#include <memory>
#include <skity/gpu/gpu_context.hpp>
#include <skity/skity.hpp>

namespace skity::example::bicubic {

std::shared_ptr<skity::Pixmap> LoadBitmap(const char* path);

std::shared_ptr<skity::Image> LoadImage(std::shared_ptr<skity::Pixmap> pixmap,
                                        skity::GPUContext* gpu_context);

// Draws the same image upscaled under bilinear, Catmull-Rom and Mitchell
// sampling side by side, so the bicubic sharpness/ringing difference is
// visible at a glance.
void DrawBicubic(skity::Canvas* canvas,
                 const std::shared_ptr<skity::Image>& image);

}  // namespace skity::example::bicubic

#endif  // EXAMPLE_CASE_BICUBIC_BICUBIC_EXAMPLE_HPP
