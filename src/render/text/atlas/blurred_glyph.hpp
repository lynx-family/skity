// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_ATLAS_BLURRED_GLYPH_HPP
#define SRC_RENDER_TEXT_ATLAS_BLURRED_GLYPH_HPP

#include <skity/macros.hpp>
#include <skity/text/glyph.hpp>
#include <vector>

// On iOS and macOS, let the compiler vectorize the generic convolution.
#if defined(__aarch64__) && defined(__ARM_NEON) && !defined(SKITY_IOS) && \
    !defined(SKITY_MACOS)
#define SKITY_GLYPH_BLUR_NEON
#elif (defined(_M_X64) || defined(__x86_64__)) && !defined(_M_ARM64EC) && \
    !defined(__arm64ec__) &&                                              \
    (defined(SKITY_WIN) || (defined(SKITY_LINUX) && !defined(SKITY_HARMONY)))
#define SKITY_GLYPH_BLUR_X64
#endif

namespace skity {

// Convolve the source alpha and return tightly packed A8 rows.
// TODO(jingle): Consider sharing the SIMD blur kernels with the new software
// renderer once it is ready.
std::vector<uint8_t> BlurGlyphMask(const GlyphBitmapData& source,
                                   const std::vector<float>& weights_x,
                                   const std::vector<float>& weights_y);

}  // namespace skity

#endif  // SRC_RENDER_TEXT_ATLAS_BLURRED_GLYPH_HPP
