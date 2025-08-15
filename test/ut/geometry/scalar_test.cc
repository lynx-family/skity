// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <cmath>
#include <skity/geometry/scalar.hpp>

// compatible with windows MSVC
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0)
#endif

#ifndef M_PI_4
#define M_PI_4 (M_PI / 4.0)
#endif

using namespace skity;

TEST(ScalarTest, FloatSignAsInt) {
  EXPECT_EQ(FloatSignAsInt(3.14f), 1);
  EXPECT_EQ(FloatSignAsInt(-2.71f), -1);
  EXPECT_EQ(FloatSignAsInt(0.0f), 0);
}

TEST(ScalarTest, RoundToInt) {
  EXPECT_EQ(RoundToInt(2.3f), 2);
  EXPECT_EQ(RoundToInt(2.5f), 3);
  EXPECT_EQ(RoundToInt(-2.5f), -3);
  EXPECT_EQ(RoundToInt(3.7f), 4);
  EXPECT_EQ(RoundToInt(-4.2f), -4);
}

TEST(ScalarTest, FloatNearlyZero) {
  EXPECT_TRUE(FloatNearlyZero(0.0f));
  EXPECT_TRUE(FloatNearlyZero(1e-13f));
  EXPECT_FALSE(FloatNearlyZero(0.5f));
  EXPECT_TRUE(FloatNearlyZero(0.1f, 0.2f));
  EXPECT_FALSE(FloatNearlyZero(0.3f, 0.2f));
}

TEST(ScalarTest, FloatFract) {
  EXPECT_NEAR(FloatFract(2.3f), 0.3f, 1e-6f);
  EXPECT_NEAR(FloatFract(-2.3f), 0.7f, 1e-6f);
  EXPECT_EQ(FloatFract(5.0f), 0.0f);
  EXPECT_NEAR(FloatFract(-3.0f), 0.0f, 1e-6f);
}

TEST(ScalarTest, FloatInterp) {
  EXPECT_EQ(FloatInterp(0.0f, 10.0f, 0.5f), 5.0f);
  EXPECT_EQ(FloatInterp(2.0f, 4.0f, 2.0f), 6.0f);
  EXPECT_EQ(FloatInterp(-1.0f, -3.0f, 0.5f), -2.0f);
}

TEST(ScalarTest, FloatInterpFunc) {
  const float keys[] = {1.0f, 2.0f, 3.0f};
  const float values[] = {10.0f, 20.0f, 30.0f};

  EXPECT_EQ(FloatInterpFunc(0.5f, keys, values, 3), 10.0f);
  EXPECT_EQ(FloatInterpFunc(3.5f, keys, values, 3), 30.0f);
  EXPECT_NEAR(FloatInterpFunc(2.5f, keys, values, 3), 25.0f, 1e-6f);
  EXPECT_NEAR(FloatInterpFunc(1.5f, keys, values, 3), 15.0f, 1e-6f);
}

TEST(ScalarTest, SkityFloatHalf) {
  EXPECT_EQ(SkityFloatHalf(4.0f), 2.0f);
  EXPECT_EQ(SkityFloatHalf(-3.0f), -1.5f);
  EXPECT_EQ(SkityFloatHalf(0.0f), 0.0f);
}

TEST(ScalarTest, FloatIsNan) {
  EXPECT_TRUE(FloatIsNan(FloatNaN));
  EXPECT_FALSE(FloatIsNan(0.0f));
  EXPECT_TRUE(FloatIsNan(std::numeric_limits<float>::quiet_NaN()));
}

TEST(ScalarTest, FloatSquare) {
  EXPECT_EQ(FloatSquare(3.0f), 9.0f);
  EXPECT_EQ(FloatSquare(-2.0f), 4.0f);
  EXPECT_EQ(FloatSquare(0.0f), 0.0f);
  EXPECT_NEAR(FloatSquare(1.5f), 2.25f, 1e-6f);
}

TEST(ScalarTest, SkityIEEEFloatDivided) {
  EXPECT_EQ(SkityIEEEFloatDivided(6.0f, 2.0f), 3.0f);
  EXPECT_TRUE(std::isinf(SkityIEEEFloatDivided(1.0f, 0.0f)));
  EXPECT_TRUE(FloatIsNan(SkityIEEEFloatDivided(0.0f, 0.0f)));
  EXPECT_EQ(SkityIEEEFloatDivided(-8.0f, 4.0f), -2.0f);
}

TEST(ScalarTest, FloatInvert) {
  EXPECT_EQ(FloatInvert(2.0f), 0.5f);
  EXPECT_EQ(FloatInvert(-0.5f), -2.0f);
  EXPECT_TRUE(std::isinf(FloatInvert(0.0f)));
}

TEST(ScalarTest, FloatIsFinite) {
  EXPECT_TRUE(FloatIsFinite(1.0f));
  EXPECT_FALSE(FloatIsFinite(FloatInfinity));
  EXPECT_FALSE(FloatIsFinite(-FloatInfinity));
  EXPECT_FALSE(!FloatIsFinite(FloatNaN));
  EXPECT_TRUE(FloatIsFinite(0.0f));
}

TEST(ScalarTest, TrigonometricFunctions) {
  // Sin tests
  EXPECT_EQ(FloatSinSnapToZero(0.0f), 0.0f);
  EXPECT_NEAR(FloatSinSnapToZero((float)M_PI_2), 1.0f, 1e-6f);
  EXPECT_EQ(FloatSinSnapToZero(1e-12f), 0.0f);
  EXPECT_NEAR(FloatSinSnapToZero((float)M_PI), 0.0f, 1e-6f);

  // Cos tests
  EXPECT_NEAR(FloatCosSnapToZero(0.0f), 1.0f, 1e-6f);
  EXPECT_EQ(FloatCosSnapToZero((float)M_PI_2), 0.0f);
  EXPECT_NEAR(FloatCosSnapToZero((float)M_PI), -1.0f, 1e-6f);

  // Tan tests
  EXPECT_EQ(FloatTanSnapToZero(0.0f), 0.0f);
  EXPECT_NEAR(FloatTanSnapToZero((float)M_PI_4), 1.0f, 1e-6f);
}

TEST(ScalarTest, FloatCopySign) {
  EXPECT_EQ(FloatCopySign(3.0f, -1.0f), -3.0f);
  EXPECT_EQ(FloatCopySign(-2.0f, -5.0f), -2.0f);
  EXPECT_EQ(FloatCopySign(-4.0f, 1.0f), 4.0f);
  EXPECT_EQ(FloatCopySign(0.0f, -0.0f), -0.0f);
}

TEST(ScalarTest, AngleConversions) {
  EXPECT_NEAR(FloatRadiansToDegrees((float)M_PI), 180.0f, 1e-6f);
  EXPECT_NEAR(FloatRadiansToDegrees((float)M_PI_2), 90.0f, 1e-6f);

  EXPECT_NEAR(FloatDegreesToRadians(180.0f), (float)M_PI, 1e-6f);
  EXPECT_NEAR(FloatDegreesToRadians(90.0f), (float)M_PI_2, 1e-6f);
  EXPECT_EQ(FloatDegreesToRadians(0.0f), 0.0f);
}
