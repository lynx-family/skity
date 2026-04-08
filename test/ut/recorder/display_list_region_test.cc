// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/recorder/display_list_region.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using skity::DisplayListRegion;
using skity::Rect;

TEST(DisplayListRegion, EmptyRegion) {
  DisplayListRegion region;
  EXPECT_TRUE(region.IsEmpty());
  EXPECT_EQ(region.GetBounds(), Rect::MakeEmpty());
  EXPECT_TRUE(region.GetRects().empty());
}

TEST(DisplayListRegion, SingleRectangle) {
  DisplayListRegion region(Rect::MakeLTRB(10, 10, 50, 50));
  EXPECT_FALSE(region.IsEmpty());
  EXPECT_EQ(region.GetBounds(), Rect::MakeLTRB(10, 10, 50, 50));
  EXPECT_THAT(region.GetRects(),
              testing::ElementsAre(Rect::MakeLTRB(10, 10, 50, 50)));
}

TEST(DisplayListRegion, NonOverlappingRectanglesNormalizedOrder) {
  DisplayListRegion region({
      Rect::MakeXYWH(0, 0, 10, 10),
      Rect::MakeXYWH(-11, -11, 10, 10),
      Rect::MakeXYWH(11, 11, 10, 10),
      Rect::MakeXYWH(-11, 0, 10, 10),
      Rect::MakeXYWH(0, 11, 10, 10),
      Rect::MakeXYWH(0, -11, 10, 10),
      Rect::MakeXYWH(11, 0, 10, 10),
      Rect::MakeXYWH(11, -11, 10, 10),
      Rect::MakeXYWH(-11, 11, 10, 10),
  });

  EXPECT_EQ(region.GetBounds(), Rect::MakeLTRB(-11, -11, 21, 21));
  EXPECT_THAT(
      region.GetRects(),
      testing::ElementsAre(
          Rect::MakeXYWH(-11, -11, 10, 10), Rect::MakeXYWH(0, -11, 10, 10),
          Rect::MakeXYWH(11, -11, 10, 10), Rect::MakeXYWH(-11, 0, 10, 10),
          Rect::MakeXYWH(0, 0, 10, 10), Rect::MakeXYWH(11, 0, 10, 10),
          Rect::MakeXYWH(-11, 11, 10, 10), Rect::MakeXYWH(0, 11, 10, 10),
          Rect::MakeXYWH(11, 11, 10, 10)));
}

TEST(DisplayListRegion, MergeTouchingRectangles) {
  DisplayListRegion region({
      Rect::MakeXYWH(0, 0, 10, 10),
      Rect::MakeXYWH(-10, -10, 10, 10),
      Rect::MakeXYWH(10, 10, 10, 10),
      Rect::MakeXYWH(-10, 0, 10, 10),
      Rect::MakeXYWH(0, 10, 10, 10),
      Rect::MakeXYWH(0, -10, 10, 10),
      Rect::MakeXYWH(10, 0, 10, 10),
      Rect::MakeXYWH(10, -10, 10, 10),
      Rect::MakeXYWH(-10, 10, 10, 10),
  });

  EXPECT_THAT(region.GetRects(),
              testing::ElementsAre(Rect::MakeXYWH(-10, -10, 30, 30)));
}

TEST(DisplayListRegion, OverlappingRectangles) {
  std::vector<Rect> rects;
  for (int i = 0; i < 6; ++i) {
    rects.push_back(Rect::MakeXYWH(10 * i, 10 * i, 50, 50));
  }

  DisplayListRegion region(rects);
  EXPECT_THAT(
      region.GetRects(),
      testing::ElementsAre(
          Rect::MakeLTRB(0, 0, 50, 10), Rect::MakeLTRB(0, 10, 60, 20),
          Rect::MakeLTRB(0, 20, 70, 30), Rect::MakeLTRB(0, 30, 80, 40),
          Rect::MakeLTRB(0, 40, 90, 50), Rect::MakeLTRB(10, 50, 100, 60),
          Rect::MakeLTRB(20, 60, 100, 70), Rect::MakeLTRB(30, 70, 100, 80),
          Rect::MakeLTRB(40, 80, 100, 90), Rect::MakeLTRB(50, 90, 100, 100)));
}

TEST(DisplayListRegion, Deband) {
  DisplayListRegion region({
      Rect::MakeXYWH(0, 0, 50, 50),
      Rect::MakeXYWH(60, 0, 20, 20),
      Rect::MakeXYWH(90, 0, 50, 50),
  });

  EXPECT_THAT(region.GetRects(true),
              testing::UnorderedElementsAre(Rect::MakeXYWH(0, 0, 50, 50),
                                            Rect::MakeXYWH(60, 0, 20, 20),
                                            Rect::MakeXYWH(90, 0, 50, 50)));
  EXPECT_THAT(region.GetRects(false),
              testing::ElementsAre(
                  Rect::MakeXYWH(0, 0, 50, 20), Rect::MakeXYWH(60, 0, 20, 20),
                  Rect::MakeXYWH(90, 0, 50, 20), Rect::MakeXYWH(0, 20, 50, 30),
                  Rect::MakeXYWH(90, 20, 50, 30)));
}

TEST(DisplayListRegion, Intersects) {
  DisplayListRegion region1({
      Rect::MakeXYWH(0, 0, 20, 20),
      Rect::MakeXYWH(20, 20, 20, 20),
  });
  DisplayListRegion region2({
      Rect::MakeXYWH(20, 0, 20, 20),
      Rect::MakeXYWH(0, 20, 20, 20),
  });

  EXPECT_FALSE(region1.Intersects(region2));
  EXPECT_FALSE(region2.Intersects(region1));
  EXPECT_TRUE(region1.Intersects(region2.GetBounds()));
  EXPECT_TRUE(region1.Intersects(Rect::MakeXYWH(0, 0, 20, 20)));
  EXPECT_FALSE(region1.Intersects(Rect::MakeXYWH(20, 0, 20, 20)));
}

TEST(DisplayListRegion, IntersectsEdges) {
  DisplayListRegion region({
      Rect::MakeXYWH(0, 0, 20, 20),
      Rect::MakeXYWH(20, 20, 20, 20),
  });

  EXPECT_FALSE(region.Intersects(Rect::MakeXYWH(-1, -1, 1, 1)));
  EXPECT_TRUE(region.Intersects(Rect::MakeXYWH(0, 0, 1, 1)));
  EXPECT_FALSE(region.Intersects(Rect::MakeXYWH(40, 40, 1, 1)));
  EXPECT_TRUE(region.Intersects(Rect::MakeXYWH(39, 39, 1, 1)));
}

TEST(DisplayListRegion, IntersectsComplex) {
  DisplayListRegion region1({
      Rect::MakeXYWH(-10, -10, 20, 20),
      Rect::MakeXYWH(-30, -30, 20, 20),
  });
  DisplayListRegion region2({
      Rect::MakeXYWH(20, 20, 5, 5),
      Rect::MakeXYWH(0, 0, 20, 20),
  });

  EXPECT_TRUE(region1.Intersects(region2));
  EXPECT_TRUE(region2.Intersects(region1));
}

TEST(DisplayListRegion, IntersectionEmpty) {
  DisplayListRegion region1({
      Rect::MakeXYWH(0, 0, 20, 20),
      Rect::MakeXYWH(20, 20, 20, 20),
  });
  DisplayListRegion region2({
      Rect::MakeXYWH(20, 0, 20, 20),
      Rect::MakeXYWH(0, 20, 20, 20),
  });

  auto intersection = DisplayListRegion::MakeIntersection(region1, region2);
  EXPECT_TRUE(intersection.IsEmpty());
  EXPECT_EQ(intersection.GetBounds(), Rect::MakeEmpty());
  EXPECT_TRUE(intersection.GetRects().empty());
}

TEST(DisplayListRegion, IntersectionEqual) {
  DisplayListRegion region1({
      Rect::MakeXYWH(0, 0, 20, 20),
      Rect::MakeXYWH(20, 20, 20, 20),
  });
  DisplayListRegion region2({
      Rect::MakeXYWH(0, 0, 20, 20),
      Rect::MakeXYWH(20, 20, 20, 20),
  });

  auto intersection = DisplayListRegion::MakeIntersection(region1, region2);
  EXPECT_EQ(intersection.GetBounds(), Rect::MakeXYWH(0, 0, 40, 40));
  EXPECT_THAT(intersection.GetRects(),
              testing::ElementsAre(Rect::MakeXYWH(0, 0, 20, 20),
                                   Rect::MakeXYWH(20, 20, 20, 20)));
}

TEST(DisplayListRegion, Intersection) {
  DisplayListRegion region1({
      Rect::MakeXYWH(0, 0, 20, 20),
  });
  DisplayListRegion region2({
      Rect::MakeXYWH(-10, -10, 20, 20),
      Rect::MakeXYWH(10, 10, 20, 20),
  });

  auto intersection = DisplayListRegion::MakeIntersection(region1, region2);
  EXPECT_EQ(intersection.GetBounds(), Rect::MakeXYWH(0, 0, 20, 20));
  EXPECT_THAT(intersection.GetRects(),
              testing::ElementsAre(Rect::MakeXYWH(0, 0, 10, 10),
                                   Rect::MakeXYWH(10, 10, 10, 10)));
}

TEST(DisplayListRegion, UnionSimple) {
  DisplayListRegion region1({
      Rect::MakeXYWH(0, 0, 20, 20),
      Rect::MakeXYWH(20, 20, 20, 20),
  });
  DisplayListRegion region2({
      Rect::MakeXYWH(20, 0, 20, 20),
      Rect::MakeXYWH(0, 20, 20, 20),
  });

  auto union_region = DisplayListRegion::MakeUnion(region1, region2);
  EXPECT_EQ(union_region.GetBounds(), Rect::MakeXYWH(0, 0, 40, 40));
  EXPECT_THAT(union_region.GetRects(),
              testing::ElementsAre(Rect::MakeXYWH(0, 0, 40, 40)));
}

TEST(DisplayListRegion, UnionSeparated) {
  DisplayListRegion region1({
      Rect::MakeXYWH(0, 0, 20, 20),
      Rect::MakeXYWH(21, 21, 20, 20),
  });
  DisplayListRegion region2({
      Rect::MakeXYWH(21, 0, 20, 20),
      Rect::MakeXYWH(0, 21, 20, 20),
  });

  auto union_region = DisplayListRegion::MakeUnion(region1, region2);
  EXPECT_EQ(union_region.GetBounds(), Rect::MakeXYWH(0, 0, 41, 41));
  EXPECT_THAT(union_region.GetRects(),
              testing::ElementsAre(Rect::MakeXYWH(0, 0, 20, 20),
                                   Rect::MakeXYWH(21, 0, 20, 20),
                                   Rect::MakeXYWH(0, 21, 20, 20),
                                   Rect::MakeXYWH(21, 21, 20, 20)));
}

TEST(DisplayListRegion, Union) {
  DisplayListRegion region1({
      Rect::MakeXYWH(-10, -10, 20, 20),
  });
  DisplayListRegion region2({
      Rect::MakeXYWH(0, 0, 20, 20),
  });

  auto union_region = DisplayListRegion::MakeUnion(region1, region2);
  EXPECT_EQ(union_region.GetBounds(), Rect::MakeXYWH(-10, -10, 30, 30));
  EXPECT_THAT(union_region.GetRects(),
              testing::ElementsAre(Rect::MakeXYWH(-10, -10, 20, 10),
                                   Rect::MakeXYWH(-10, 0, 30, 10),
                                   Rect::MakeXYWH(0, 10, 20, 10)));
}

TEST(DisplayListRegion, UnionEmpty) {
  {
    DisplayListRegion region1;
    DisplayListRegion region2;
    auto union_region = DisplayListRegion::MakeUnion(region1, region2);
    EXPECT_TRUE(union_region.IsEmpty());
    EXPECT_EQ(union_region.GetBounds(), Rect::MakeEmpty());
    EXPECT_TRUE(union_region.GetRects().empty());
  }

  {
    DisplayListRegion region1;
    DisplayListRegion region2(Rect::MakeXYWH(0, 0, 20, 20));
    auto union_region = DisplayListRegion::MakeUnion(region1, region2);
    EXPECT_EQ(union_region.GetBounds(), Rect::MakeXYWH(0, 0, 20, 20));
    EXPECT_THAT(union_region.GetRects(),
                testing::ElementsAre(Rect::MakeXYWH(0, 0, 20, 20)));
  }

  {
    DisplayListRegion region1(Rect::MakeXYWH(0, 0, 20, 20));
    DisplayListRegion region2;
    auto union_region = DisplayListRegion::MakeUnion(region1, region2);
    EXPECT_EQ(union_region.GetBounds(), Rect::MakeXYWH(0, 0, 20, 20));
    EXPECT_THAT(union_region.GetRects(),
                testing::ElementsAre(Rect::MakeXYWH(0, 0, 20, 20)));
  }
}

}  // namespace
