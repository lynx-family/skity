// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/shape.hpp"

#include <gtest/gtest.h>

#include <skity/skity.hpp>

#include "skity/geometry/rrect.hpp"
#include "skity/graphic/path.hpp"

TEST(Shape, Path) {
  skity::Path path;
  path.MoveTo(0.0, 0.0);
  path.LineTo(10.0, 10.0);
  path.Close();

  skity::Shape shape(&path);
  EXPECT_TRUE(shape.IsPath());
  EXPECT_FALSE(shape.IsRRect());
  EXPECT_EQ(shape.GetPath(), &path);
  EXPECT_EQ(shape.GetBounds(), path.GetBounds());
}

TEST(Shape, RRect) {
  skity::RRect rrect;
  rrect.SetRectXY(skity::Rect::MakeLTRB(10, 20, 30, 50), 10, 20);

  skity::Shape shape(&rrect);
  EXPECT_FALSE(shape.IsPath());
  EXPECT_TRUE(shape.IsRRect());
  EXPECT_EQ(shape.GetRRect(), &rrect);
  EXPECT_EQ(shape.GetBounds(), rrect.GetBounds());
}
