// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/geometry/rect.hpp>

#include "gtest/gtest.h"

TEST(Rect, Intersect) {
  skity::Rect rect1 = skity::Rect::MakeLTRB(0, 0, 100, 100);
  skity::Rect rect2 = skity::Rect::MakeLTRB(200, 200, 300, 300);
  skity::Rect rect3 = skity::Rect::MakeLTRB(150, 250, 350, 260);

  EXPECT_FALSE(rect1.Intersect(rect2));
  EXPECT_EQ(skity::Rect::MakeLTRB(0, 0, 100, 100), rect1);

  EXPECT_FALSE(rect1.Intersect(skity::Rect::MakeEmpty()));
  EXPECT_EQ(skity::Rect::MakeLTRB(0, 0, 100, 100), rect1);

  EXPECT_TRUE(rect2.Intersect(rect3));
  EXPECT_EQ(skity::Rect::MakeLTRB(200, 250, 300, 260), rect2);
}
