// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/graphic/paint.hpp>

#include "gtest/gtest.h"

TEST(Paint, DefaultConstructor) {
  skity::Paint paint;

  // Test default values
  EXPECT_EQ(paint.GetStyle(), skity::Paint::kFill_Style);
  EXPECT_EQ(paint.GetStrokeWidth(), 1.0f);
  EXPECT_EQ(paint.GetStrokeMiter(), skity::Paint::kDefaultMiterLimit);
  EXPECT_EQ(paint.GetStrokeCap(), skity::Paint::kDefault_Cap);
  EXPECT_EQ(paint.GetStrokeJoin(), skity::Paint::kDefault_Join);
  EXPECT_FALSE(paint.IsAntiAlias());
  EXPECT_EQ(paint.GetTextSize(), 14.f);
  EXPECT_FALSE(paint.IsSDFForSmallText());
  EXPECT_EQ(paint.GetFontThreshold(), 256.f);
  EXPECT_EQ(paint.GetBlendMode(), skity::BlendMode::kDefault);
  EXPECT_FALSE(paint.IsAdjustStroke());
}

TEST(Paint, CopyConstructor) {
  skity::Paint paint1;
  paint1.SetStyle(skity::Paint::kStroke_Style);
  paint1.SetStrokeWidth(2.5f);
  paint1.SetAntiAlias(true);

  skity::Paint paint2(paint1);

  EXPECT_EQ(paint2.GetStyle(), skity::Paint::kStroke_Style);
  EXPECT_EQ(paint2.GetStrokeWidth(), 2.5f);
  EXPECT_TRUE(paint2.IsAntiAlias());
}

TEST(Paint, AssignmentOperator) {
  skity::Paint paint1;
  paint1.SetStyle(skity::Paint::kStrokeAndFill_Style);
  paint1.SetStrokeWidth(3.0f);

  skity::Paint paint2;
  paint2 = paint1;

  EXPECT_EQ(paint2.GetStyle(), skity::Paint::kStrokeAndFill_Style);
  EXPECT_EQ(paint2.GetStrokeWidth(), 3.0f);
}

TEST(Paint, Reset) {
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(5.0f);
  paint.SetAntiAlias(true);

  paint.Reset();

  EXPECT_EQ(paint.GetStyle(), skity::Paint::kFill_Style);
  EXPECT_EQ(paint.GetStrokeWidth(), 1.0f);
  EXPECT_FALSE(paint.IsAntiAlias());
}

TEST(Paint, Style) {
  skity::Paint paint;

  paint.SetStyle(skity::Paint::kFill_Style);
  EXPECT_EQ(paint.GetStyle(), skity::Paint::kFill_Style);

  paint.SetStyle(skity::Paint::kStroke_Style);
  EXPECT_EQ(paint.GetStyle(), skity::Paint::kStroke_Style);

  paint.SetStyle(skity::Paint::kStrokeAndFill_Style);
  EXPECT_EQ(paint.GetStyle(), skity::Paint::kStrokeAndFill_Style);

  paint.SetStyle(skity::Paint::kStrokeThenFill_Style);
  EXPECT_EQ(paint.GetStyle(), skity::Paint::kStrokeThenFill_Style);
}

TEST(Paint, StrokeWidth) {
  skity::Paint paint;

  paint.SetStrokeWidth(0.5f);
  EXPECT_EQ(paint.GetStrokeWidth(), 0.5f);

  paint.SetStrokeWidth(10.0f);
  EXPECT_EQ(paint.GetStrokeWidth(), 10.0f);
}

TEST(Paint, StrokeMiter) {
  skity::Paint paint;

  paint.SetStrokeMiter(2.0f);
  EXPECT_EQ(paint.GetStrokeMiter(), 2.0f);

  paint.SetStrokeMiter(8.0f);
  EXPECT_EQ(paint.GetStrokeMiter(), 8.0f);
}

TEST(Paint, StrokeCap) {
  skity::Paint paint;

  paint.SetStrokeCap(skity::Paint::kButt_Cap);
  EXPECT_EQ(paint.GetStrokeCap(), skity::Paint::kButt_Cap);

  paint.SetStrokeCap(skity::Paint::kRound_Cap);
  EXPECT_EQ(paint.GetStrokeCap(), skity::Paint::kRound_Cap);

  paint.SetStrokeCap(skity::Paint::kSquare_Cap);
  EXPECT_EQ(paint.GetStrokeCap(), skity::Paint::kSquare_Cap);
}

TEST(Paint, StrokeJoin) {
  skity::Paint paint;

  paint.SetStrokeJoin(skity::Paint::kMiter_Join);
  EXPECT_EQ(paint.GetStrokeJoin(), skity::Paint::kMiter_Join);

  paint.SetStrokeJoin(skity::Paint::kRound_Join);
  EXPECT_EQ(paint.GetStrokeJoin(), skity::Paint::kRound_Join);

  paint.SetStrokeJoin(skity::Paint::kBevel_Join);
  EXPECT_EQ(paint.GetStrokeJoin(), skity::Paint::kBevel_Join);
}

TEST(Paint, StrokeColorVector) {
  skity::Paint paint;

  paint.SetStrokeColor(0.5f, 0.6f, 0.7f, 0.8f);
  skity::Vector color = paint.GetStrokeColor();

  EXPECT_FLOAT_EQ(color.x, 0.5f);
  EXPECT_FLOAT_EQ(color.y, 0.6f);
  EXPECT_FLOAT_EQ(color.z, 0.7f);
  EXPECT_FLOAT_EQ(color.w, 0.8f);

  skity::Vector color2(0.1f, 0.2f, 0.3f, 0.4f);
  paint.SetStrokeColor(color2);
  color = paint.GetStrokeColor();

  EXPECT_FLOAT_EQ(color.x, 0.1f);
  EXPECT_FLOAT_EQ(color.y, 0.2f);
  EXPECT_FLOAT_EQ(color.z, 0.3f);
  EXPECT_FLOAT_EQ(color.w, 0.4f);
}

TEST(Paint, StrokeColorColor) {
  skity::Paint paint;

  skity::Color color = skity::ColorSetARGB(0xFF, 0xAA, 0xBB, 0xCC);
  paint.SetStrokeColor(color);

  skity::Vector strokeColor = paint.GetStrokeColor();
  EXPECT_NEAR(strokeColor.x, 0xAA / 255.0f, 0.01f);
  EXPECT_NEAR(strokeColor.y, 0xBB / 255.0f, 0.01f);
  EXPECT_NEAR(strokeColor.z, 0xCC / 255.0f, 0.01f);
  EXPECT_NEAR(strokeColor.w, 0xFF / 255.0f, 0.01f);
}

TEST(Paint, FillColorVector) {
  skity::Paint paint;

  paint.SetFillColor(0.3f, 0.4f, 0.5f, 0.6f);
  skity::Vector color = paint.GetFillColor();

  EXPECT_FLOAT_EQ(color.x, 0.3f);
  EXPECT_FLOAT_EQ(color.y, 0.4f);
  EXPECT_FLOAT_EQ(color.z, 0.5f);
  EXPECT_FLOAT_EQ(color.w, 0.6f);

  skity::Vector color2(0.7f, 0.8f, 0.9f, 1.0f);
  paint.SetFillColor(color2);
  color = paint.GetFillColor();

  EXPECT_FLOAT_EQ(color.x, 0.7f);
  EXPECT_FLOAT_EQ(color.y, 0.8f);
  EXPECT_FLOAT_EQ(color.z, 0.9f);
  EXPECT_FLOAT_EQ(color.w, 1.0f);
}

TEST(Paint, FillColorColor) {
  skity::Paint paint;

  skity::Color color = skity::ColorSetARGB(0xFF, 0x11, 0x22, 0x33);
  paint.SetFillColor(color);

  skity::Vector fillColor = paint.GetFillColor();
  EXPECT_NEAR(fillColor.x, 0x11 / 255.0f, 0.01f);
  EXPECT_NEAR(fillColor.y, 0x22 / 255.0f, 0.01f);
  EXPECT_NEAR(fillColor.z, 0x33 / 255.0f, 0.01f);
  EXPECT_NEAR(fillColor.w, 0xFF / 255.0f, 0.01f);
}

TEST(Paint, SetColor) {
  skity::Paint paint;

  skity::Color color = skity::ColorSetARGB(0xFF, 0x44, 0x55, 0x66);
  paint.SetColor(color);

  // SetColor should set both stroke and fill colors
  skity::Vector strokeColor = paint.GetStrokeColor();
  skity::Vector fillColor = paint.GetFillColor();

  EXPECT_NEAR(strokeColor.x, 0x44 / 255.0f, 0.01f);
  EXPECT_NEAR(fillColor.x, 0x44 / 255.0f, 0.01f);
}

TEST(Paint, GetColor) {
  skity::Paint paint;

  // Default style is kFill_Style, so GetColor should return fill color
  paint.SetFillColor(skity::ColorSetARGB(0xFF, 0x77, 0x88, 0x99));
  skity::Color color = paint.GetColor();

  EXPECT_EQ(ColorGetR(color), 0x77);
  EXPECT_EQ(ColorGetG(color), 0x88);
  EXPECT_EQ(ColorGetB(color), 0x99);

  // Set style to stroke, GetColor should return stroke color
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeColor(skity::ColorSetARGB(0xFF, 0xAA, 0xBB, 0xCC));
  color = paint.GetColor();

  EXPECT_EQ(ColorGetR(color), 0xAA);
  EXPECT_EQ(ColorGetG(color), 0xBB);
  EXPECT_EQ(ColorGetB(color), 0xCC);
}

TEST(Paint, GetColor4f) {
  skity::Paint paint;

  paint.SetFillColor(0.5f, 0.6f, 0.7f, 0.8f);
  skity::Color4f color = paint.GetColor4f();

  EXPECT_FLOAT_EQ(color.r, 0.5f);
  EXPECT_FLOAT_EQ(color.g, 0.6f);
  EXPECT_FLOAT_EQ(color.b, 0.7f);
  EXPECT_FLOAT_EQ(color.a, 0.8f);
}

TEST(Paint, AntiAlias) {
  skity::Paint paint;

  EXPECT_FALSE(paint.IsAntiAlias());

  paint.SetAntiAlias(true);
  EXPECT_TRUE(paint.IsAntiAlias());

  paint.SetAntiAlias(false);
  EXPECT_FALSE(paint.IsAntiAlias());
}

TEST(Paint, TextSize) {
  skity::Paint paint;

  EXPECT_EQ(paint.GetTextSize(), 14.f);

  paint.SetTextSize(24.f);
  EXPECT_EQ(paint.GetTextSize(), 24.f);

  // Invalid text size (<=0) should be ignored
  paint.SetTextSize(-5.f);
  EXPECT_EQ(paint.GetTextSize(), 24.f);

  paint.SetTextSize(0.f);
  EXPECT_EQ(paint.GetTextSize(), 24.f);
}

TEST(Paint, SDFForSmallText) {
  skity::Paint paint;

  EXPECT_FALSE(paint.IsSDFForSmallText());

  paint.SetSDFForSmallText(true);
  EXPECT_TRUE(paint.IsSDFForSmallText());

  paint.SetSDFForSmallText(false);
  EXPECT_FALSE(paint.IsSDFForSmallText());
}

TEST(Paint, FontThreshold) {
  skity::Paint paint;

  EXPECT_EQ(paint.GetFontThreshold(), 256.f);

  paint.SetFontThreshold(512.f);
  EXPECT_EQ(paint.GetFontThreshold(), 512.f);

  paint.SetFontThreshold(128.f);
  EXPECT_EQ(paint.GetFontThreshold(), 128.f);
}

TEST(Paint, AlphaF) {
  skity::Paint paint;

  paint.SetAlphaF(0.5f);
  EXPECT_FLOAT_EQ(paint.GetAlphaF(), 0.5f);

  // Alpha should be clamped to [0, 1]
  paint.SetAlphaF(1.5f);
  EXPECT_FLOAT_EQ(paint.GetAlphaF(), 1.0f);

  paint.SetAlphaF(-0.5f);
  EXPECT_FLOAT_EQ(paint.GetAlphaF(), 0.0f);
}

TEST(Paint, Alpha) {
  skity::Paint paint;

  paint.SetAlpha(128);
  EXPECT_EQ(paint.GetAlpha(), 128);

  paint.SetAlpha(255);
  EXPECT_EQ(paint.GetAlpha(), 255);

  paint.SetAlpha(0);
  EXPECT_EQ(paint.GetAlpha(), 0);
}

TEST(Paint, BlendMode) {
  skity::Paint paint;

  EXPECT_EQ(paint.GetBlendMode(), skity::BlendMode::kDefault);

  paint.SetBlendMode(skity::BlendMode::kSrc);
  EXPECT_EQ(paint.GetBlendMode(), skity::BlendMode::kSrc);

  paint.SetBlendMode(skity::BlendMode::kDst);
  EXPECT_EQ(paint.GetBlendMode(), skity::BlendMode::kDst);
}

TEST(Paint, AdjustStroke) {
  skity::Paint paint;

  EXPECT_FALSE(paint.IsAdjustStroke());

  paint.SetAdjustStroke(true);
  EXPECT_TRUE(paint.IsAdjustStroke());

  paint.SetAdjustStroke(false);
  EXPECT_FALSE(paint.IsAdjustStroke());
}

TEST(Paint, EqualityOperator) {
  skity::Paint paint1;
  skity::Paint paint2;

  EXPECT_TRUE(paint1 == paint2);

  paint1.SetStyle(skity::Paint::kStroke_Style);
  EXPECT_FALSE(paint1 == paint2);

  paint2.SetStyle(skity::Paint::kStroke_Style);
  EXPECT_TRUE(paint1 == paint2);
}

TEST(Paint, InequalityOperator) {
  skity::Paint paint1;
  skity::Paint paint2;

  EXPECT_FALSE(paint1 != paint2);

  paint1.SetStrokeWidth(5.0f);
  EXPECT_TRUE(paint1 != paint2);

  paint2.SetStrokeWidth(5.0f);
  EXPECT_FALSE(paint1 != paint2);
}

TEST(Paint, ComputeFastBounds) {
  skity::Paint paint;
  skity::Rect rect = skity::Rect::MakeLTRB(10, 20, 30, 40);

  // Fill style should not expand bounds
  paint.SetStyle(skity::Paint::kFill_Style);
  skity::Rect bounds = paint.ComputeFastBounds(rect);
  EXPECT_EQ(bounds.Left(), rect.Left());
  EXPECT_EQ(bounds.Top(), rect.Top());
  EXPECT_EQ(bounds.Right(), rect.Right());
  EXPECT_EQ(bounds.Bottom(), rect.Bottom());

  // Stroke style should expand bounds by stroke width
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(4.0f);
  bounds = paint.ComputeFastBounds(rect);
  EXPECT_LT(bounds.Left(), rect.Left());
  EXPECT_LT(bounds.Top(), rect.Top());
  EXPECT_GT(bounds.Right(), rect.Right());
  EXPECT_GT(bounds.Bottom(), rect.Bottom());
}

TEST(Paint, CanComputeFastBounds) {
  skity::Paint paint;
  EXPECT_TRUE(paint.CanComputeFastBounds());
}
