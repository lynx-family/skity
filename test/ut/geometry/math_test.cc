// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/geometry/math.hpp"

#include <gtest/gtest.h>

#include <array>
#include <skity/geometry/point.hpp>

TEST(MATH, test_infinit) {
  float nan = std::asin(2);
  float inf = std::numeric_limits<float>::infinity();
  float big = 3.40282e+038f;

  EXPECT_TRUE(!skity::FloatIsNan(inf));
  EXPECT_TRUE(!skity::FloatIsNan(-inf));
  EXPECT_TRUE(!skity::FloatIsFinite(inf));
  EXPECT_TRUE(!skity::FloatIsFinite(-inf));

  EXPECT_TRUE(skity::FloatIsNan(nan));
  EXPECT_TRUE(!skity::FloatIsNan(big));
  EXPECT_TRUE(!skity::FloatIsNan(-big));
  EXPECT_TRUE(!skity::FloatIsNan(0));

  EXPECT_TRUE(skity::FloatIsFinite(big));
  EXPECT_TRUE(skity::FloatIsFinite(-big));
  EXPECT_TRUE(skity::FloatIsFinite(0));
}

TEST(MATH, Orientation) {
  skity::Point p1{1, 1, 0, 0};
  skity::Point p2{2, 2, 0, 0};
  skity::Point p3{3, 1, 0, 0};

  skity::Orientation orientation = skity::CalculateOrientation(p1, p2, p3);

  EXPECT_EQ(orientation, skity::Orientation::kClockWise);

  skity::Point p4{-2, -2, 0, 0};

  skity::Orientation orientation2 = skity::CalculateOrientation(p1, p4, p3);
  EXPECT_EQ(orientation2, skity::Orientation::kAntiClockWise);
}

TEST(MATH, DivCeil) {
  EXPECT_EQ(skity::DivCeil(1, 2), 1);
  EXPECT_EQ(skity::DivCeil(2, 2), 1);
  EXPECT_EQ(skity::DivCeil(3, 2), 2);

  EXPECT_EQ(skity::DivCeil(32, 8), 4);
  EXPECT_EQ(skity::DivCeil(35, 8), 5);
  EXPECT_EQ(skity::DivCeil(0, 8), 0);
}
