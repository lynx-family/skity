// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_COMMON_COLOR_UTILS_HPP
#define TEST_BENCH_COMMON_COLOR_UTILS_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace skity {

inline void UnpremultiplyAlpha(uint8_t *data, size_t pixelCount) {
  for (size_t i = 0; i < pixelCount; ++i) {
    uint8_t *px = data + i * 4;

    uint8_t A = px[3];
    if (A == 0 || A == 255) {
      continue;
    }

    px[0] =
        static_cast<uint8_t>(std::min(255, (px[0] * 255 + A / 2) / A));  // R
    px[1] =
        static_cast<uint8_t>(std::min(255, (px[1] * 255 + A / 2) / A));  // G
    px[2] =
        static_cast<uint8_t>(std::min(255, (px[2] * 255 + A / 2) / A));  // B
  }
}

}  // namespace skity

#endif  // TEST_BENCH_COMMON_COLOR_UTILS_HPP
