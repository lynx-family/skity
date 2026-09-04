// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <skity/graphic/color.hpp>

#include "src/text/ports/win/scaler_context_win.hpp"

namespace skity {
namespace {

TEST(A8MaskGammaTest, SrgbPreblendAdjustsCoverageAndPreservesPadding) {
  std::array<uint8_t, 8> pixels = {0, 16, 32, 0xEE, 64, 128, 255, 0xDD};
  GlyphBitmapData image;
  image.width = 3.f;
  image.height = 2.f;
  image.buffer = pixels.data();
  image.row_bytes = 4;
  image.format = BitmapFormat::kGray8;

  ScalerContextDesc desc{};
  desc.foreground_color = ColorSetRGB(246, 246, 246);
  ApplyA8MaskGammaForWindows(&image, desc);

  const std::array<uint8_t, 8> expected = {0,   71,  99,  0xEE,
                                           137, 188, 255, 0xDD};
  EXPECT_EQ(pixels, expected);
}

TEST(A8MaskGammaTest, RawMaskPreservesCoverageAndPadding) {
  const std::array<uint8_t, 8> expected = {0, 16, 32, 0xEE, 64, 128, 255, 0xDD};
  std::array<uint8_t, 8> pixels = expected;
  GlyphBitmapData image;
  image.width = 3.f;
  image.height = 2.f;
  image.buffer = pixels.data();
  image.row_bytes = 4;
  image.format = BitmapFormat::kGray8;

  ScalerContextDesc desc{};
  desc.foreground_color = ColorSetRGB(246, 246, 246);
  desc.scaler_flags |= ScalerContextDesc::kGenerateRawA8MaskFlag;
  ApplyA8MaskGammaForWindows(&image, desc);

  EXPECT_EQ(pixels, expected);
}

}  // namespace
}  // namespace skity
