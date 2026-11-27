// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/blurred_glyph.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace skity {
namespace {

#if defined(SKITY_GLYPH_BLUR_NEON) || defined(SKITY_GLYPH_BLUR_X64)
TEST(BlurredGlyphTest, PreservesHalfwayRoundingOnBothAxes) {
  const std::vector<float> kernel{0.28260896f, 0.43478212f, 0.28260896f};
  // In this tap order the separately rounded sum is 133.5. Fusing the
  // multiply and add gives the float just below 133.5.
  uint8_t row[] = {206, 153, 31};
  GlyphBitmapData source{};
  source.format = BitmapFormat::kGray8;
  source.buffer = row;
  source.width = 3;
  source.height = 1;
  EXPECT_EQ(BlurGlyphMask(source, kernel, {1.f})[2], 134);

  uint8_t column[] = {31, 153, 206};
  source.buffer = column;
  source.width = 1;
  source.height = 3;
  EXPECT_EQ(BlurGlyphMask(source, {1.f}, kernel)[2], 134);
}
#endif

TEST(BlurredGlyphTest, RoundsCoverageLikeStdRound) {
  // Distinct values in both halves of each vector expose packing permutations.
  std::vector<uint8_t> alpha(67);
  for (size_t i = 0; i < alpha.size(); ++i) {
    alpha[i] = static_cast<uint8_t>(2 * i + 1);
  }
  GlyphBitmapData source{};
  source.format = BitmapFormat::kGray8;
  source.buffer = alpha.data();
  source.width = static_cast<float>(alpha.size());
  source.height = 1;
  for (float w : {0.5f, std::nextafter(0.5f, 0.f), std::nextafter(0.5f, 1.f)}) {
    std::vector<uint8_t> expected;
    for (auto a : alpha) {
      expected.push_back(static_cast<uint8_t>(std::round(a * w)));
    }
    EXPECT_EQ(BlurGlyphMask(source, {w}, {1.f}), expected);
    EXPECT_EQ(BlurGlyphMask(source, {1.f}, {w}), expected);
  }
}

TEST(BlurredGlyphTest, ReadsPaddedSourceAndWritesTightRowsAtVectorEdges) {
  constexpr int kx[] = {2, 1, 1};
  constexpr int ky[] = {1, 1, 1, 2, 3};
  for (auto format :
       {BitmapFormat::kGray8, BitmapFormat::kRGBA8, BitmapFormat::kBGRA8}) {
    const int bpp = format == BitmapFormat::kGray8 ? 1 : 4;
    for (int width = 1; width <= 97; ++width) {
      SCOPED_TRACE(::testing::Message()
                   << width << " pixels, " << bpp << " B/pixel");
      const int height = 3;
      const int stride = width * bpp + 7;
      // Stride includes padding between rows only. The last pixel ends the
      // buffer.
      std::vector<uint8_t> input((height - 1) * stride + width * bpp, 255);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          input[y * stride + x * bpp + bpp - 1] =
              (x == width - 1 && y == 1) ? 160 : 0;
        }
      }
      GlyphBitmapData source{};
      source.buffer = input.data();
      source.width = width;
      source.height = height;
      source.format = format;
      source.row_bytes = stride;
      auto result = BlurGlyphMask(source, {.5f, .25f, .25f},
                                  {.125f, .125f, .125f, .25f, .375f});

      // A single impulse at the end of the middle row gives 160 * kx/4 * ky/8.
      std::vector<uint8_t> expected((width + 2) * (height + 4));
      for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 3; ++x) {
          expected[(1 + y) * (width + 2) + (width - 1 + x)] = 5 * kx[x] * ky[y];
        }
      }
      EXPECT_EQ(expected, result);
    }
  }
}

}  // namespace
}  // namespace skity
