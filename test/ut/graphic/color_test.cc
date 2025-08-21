// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/graphic/color.hpp>

#include "gtest/gtest.h"

TEST(Color, SetAndGet) {
  skity::Color c = skity::ColorSetARGB(0x80, 0xFF, 0xAA, 0x55);
  EXPECT_EQ(c, 0x80FFAA55);
  EXPECT_EQ(ColorGetA(c), 0x80);
  EXPECT_EQ(ColorGetR(c), 0xFF);
  EXPECT_EQ(ColorGetG(c), 0xAA);
  EXPECT_EQ(ColorGetB(c), 0x55);
}

TEST(Color, SetRGB) {
  skity::Color c = skity::ColorSetARGB(0xFF, 0x12, 0x34, 0x56);
  EXPECT_EQ(c, 0xFF123456);
  EXPECT_EQ(ColorGetA(c), 0xFF);
  EXPECT_EQ(ColorGetR(c), 0x12);
}

TEST(Color, SetAlpha) {
  skity::Color c1 = skity::ColorSetARGB(0xFF, 0x11, 0x22, 0x33);
  skity::Color c2 = skity::ColorSetA(c1, 0x44);
  EXPECT_EQ(c2, 0x44112233);
}

TEST(Color, Color4fConversion) {
  skity::Color c1 = skity::ColorSetARGB(255, 128, 64, 32);
  skity::Color4f c4f = skity::Color4fFromColor(c1);

  // Test conversion to float with a small tolerance
  EXPECT_NEAR(c4f.r, 128.f / 255.f, 1.f / 255.f);
  EXPECT_NEAR(c4f.g, 64.f / 255.f, 1.f / 255.f);
  EXPECT_NEAR(c4f.b, 32.f / 255.f, 1.f / 255.f);
  EXPECT_NEAR(c4f.a, 1.f, 1.f / 255.f);

  // Test round-trip conversion
  skity::Color c2 = skity::Color4fToColor(c4f);
  EXPECT_EQ(c1, c2);

  // Test clamping and truncation in Color4fToColor
  skity::Color4f c4f_out_of_bounds{-0.5f, 1.5f, 0.7f, 0.2f};
  skity::Color c3 = skity::Color4fToColor(c4f_out_of_bounds);
  EXPECT_EQ(ColorGetR(c3), 0);    // clamped from -0.5 to 0
  EXPECT_EQ(ColorGetG(c3), 255);  // clamped from 1.5 to 1.0 -> 255
  EXPECT_EQ(ColorGetB(c3), 178);  // 0.7 * 255 = 178.5 -> truncated to 178
  EXPECT_EQ(ColorGetA(c3), 51);   // 0.2 * 255 = 51.0 -> 51
}

TEST(Color, HSLAToColor) {
  // Red (H=0 or 360)
  skity::Color c_red = skity::ColorMakeFromHSLA(0.f, 1.f, 0.5f, 255);
  EXPECT_EQ(ColorGetR(c_red), 255);
  EXPECT_EQ(ColorGetG(c_red), 0);
  EXPECT_EQ(ColorGetB(c_red), 0);

  // Green (H=120)
  skity::Color c_green =
      skity::ColorMakeFromHSLA(120.f / 360.f, 1.f, 0.5f, 255);
  EXPECT_EQ(ColorGetR(c_green), 0);
  EXPECT_EQ(ColorGetG(c_green), 255);
  EXPECT_EQ(ColorGetB(c_green), 0);

  // Blue (H=240)
  skity::Color c_blue = skity::ColorMakeFromHSLA(240.f / 360.f, 1.f, 0.5f, 255);
  EXPECT_EQ(ColorGetR(c_blue), 0);
  EXPECT_EQ(ColorGetG(c_blue), 0);
  EXPECT_EQ(ColorGetB(c_blue), 255);

  // Gray (S=0)
  skity::Color c_gray = skity::ColorMakeFromHSLA(90.f / 360.f, 0.f, 0.5f, 255);
  // static_cast truncates the decimal part,127.5 -> 127.
  EXPECT_EQ(ColorGetR(c_gray), 127);
  EXPECT_EQ(ColorGetG(c_gray), 127);
  EXPECT_EQ(ColorGetB(c_gray), 127);

  // White (L=1.0)
  skity::Color c_white = skity::ColorMakeFromHSLA(0.f, 0.f, 1.0f, 255);
  EXPECT_EQ(c_white, skity::Color_WHITE);
}

TEST(Color, PredefinedColors) {
  EXPECT_EQ(skity::Color_RED, 0xFFFF0000);
  EXPECT_EQ(skity::Color_GREEN, 0xFF00FF00);
  EXPECT_EQ(skity::Color_BLUE, 0xFF0000FF);
  EXPECT_EQ(skity::Color_WHITE, 0xFFFFFFFF);
  EXPECT_EQ(skity::Color_BLACK, 0xFF000000);
  EXPECT_EQ(skity::Color_TRANSPARENT, 0x00000000);
  EXPECT_EQ(skity::Color_GRAY, 0xFF888888);
  EXPECT_EQ(skity::Color_YELLOW, 0xFFFFFF00);
}
