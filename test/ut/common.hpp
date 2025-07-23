// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/geometry/matrix.hpp>

#include "gtest/gtest.h"
#include "src/geometry/math.hpp"

static void EXPECT_MATRIX_EQ(const skity::Matrix& a, const skity::Matrix& b) {
  for (size_t i = 0; i < 4; i++) {
    for (size_t j = 0; j < 4; j++) {
      EXPECT_TRUE(skity::FloatNearlyZero(a[i][j] - b[i][j]));
    }
  }
}

static void EXPECT_MATRIX_EQ(const skity::Matrix& a, const glm::mat4& b) {
  for (size_t i = 0; i < 4; i++) {
    for (size_t j = 0; j < 4; j++) {
      EXPECT_TRUE(skity::FloatNearlyZero(a[i][j] - b[i][j]));
    }
  }
}
