// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
#include <limits>
#include <skity/geometry/rect.hpp>

#include "gtest/gtest.h"

using namespace skity;

// ============================================================================
// Constructors and Operators Tests
// ============================================================================

TEST(Rect, DefaultConstructor) {
  Rect rect;
  EXPECT_EQ(rect.Left(), 0.0f);
  EXPECT_EQ(rect.Top(), 0.0f);
  EXPECT_EQ(rect.Right(), 0.0f);
  EXPECT_EQ(rect.Bottom(), 0.0f);
}

TEST(Rect, ParameterizedConstructor) {
  Rect rect(10.0f, 20.0f, 100.0f, 200.0f);
  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, CopyConstructor) {
  Rect rect1(10.0f, 20.0f, 100.0f, 200.0f);
  Rect rect2(rect1);
  EXPECT_EQ(rect1, rect2);
  EXPECT_EQ(rect2.Left(), 10.0f);
  EXPECT_EQ(rect2.Top(), 20.0f);
  EXPECT_EQ(rect2.Right(), 100.0f);
  EXPECT_EQ(rect2.Bottom(), 200.0f);
}

TEST(Rect, MoveConstructor) {
  Rect rect1(10.0f, 20.0f, 100.0f, 200.0f);
  Rect rect2(std::move(rect1));
  EXPECT_EQ(rect2.Left(), 10.0f);
  EXPECT_EQ(rect2.Top(), 20.0f);
  EXPECT_EQ(rect2.Right(), 100.0f);
  EXPECT_EQ(rect2.Bottom(), 200.0f);
}

TEST(Rect, EqualityOperator) {
  Rect rect1(10.0f, 20.0f, 100.0f, 200.0f);
  Rect rect2(10.0f, 20.0f, 100.0f, 200.0f);
  Rect rect3(10.0f, 20.0f, 100.0f, 201.0f);

  EXPECT_TRUE(rect1 == rect2);
  EXPECT_FALSE(rect1 == rect3);
}

TEST(Rect, InequalityOperator) {
  Rect rect1(10.0f, 20.0f, 100.0f, 200.0f);
  Rect rect2(10.0f, 20.0f, 100.0f, 200.0f);
  Rect rect3(10.0f, 20.0f, 100.0f, 201.0f);

  EXPECT_FALSE(rect1 != rect2);
  EXPECT_TRUE(rect1 != rect3);
}

TEST(Rect, AssignmentOperator) {
  Rect rect1(10.0f, 20.0f, 100.0f, 200.0f);
  Rect rect2;
  rect2 = rect1;
  EXPECT_EQ(rect1, rect2);
}

// ============================================================================
// Accessor Methods Tests
// ============================================================================

TEST(Rect, AccessorMethods) {
  Rect rect(10.0f, 20.0f, 100.0f, 200.0f);

  EXPECT_EQ(rect.X(), 10.0f);
  EXPECT_EQ(rect.Y(), 20.0f);
  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, WidthHeight) {
  Rect rect(10.0f, 20.0f, 110.0f, 220.0f);

  EXPECT_EQ(rect.Width(), 100.0f);
  EXPECT_EQ(rect.Height(), 200.0f);
}

TEST(Rect, WidthHeightNegative) {
  // Test with unsorted rect (negative width/height)
  Rect rect(100.0f, 200.0f, 10.0f, 20.0f);

  EXPECT_EQ(rect.Width(), -90.0f);
  EXPECT_EQ(rect.Height(), -180.0f);
}

TEST(Rect, CenterXY) {
  Rect rect(0.0f, 0.0f, 100.0f, 200.0f);

  EXPECT_FLOAT_EQ(rect.CenterX(), 50.0f);
  EXPECT_FLOAT_EQ(rect.CenterY(), 100.0f);
}

TEST(Rect, CenterXYNonZeroOrigin) {
  Rect rect(10.0f, 20.0f, 110.0f, 220.0f);

  EXPECT_FLOAT_EQ(rect.CenterX(), 60.0f);
  EXPECT_FLOAT_EQ(rect.CenterY(), 120.0f);
}

// ============================================================================
// State Checking Methods Tests
// ============================================================================

TEST(Rect, IsEmpty) {
  EXPECT_TRUE(Rect::MakeEmpty().IsEmpty());
  EXPECT_TRUE(Rect(0, 0, 0, 0).IsEmpty());
  EXPECT_TRUE(Rect(10, 10, 10, 10).IsEmpty());
  EXPECT_TRUE(Rect(10, 10, 5, 20).IsEmpty());   // left >= right
  EXPECT_TRUE(Rect(10, 20, 15, 20).IsEmpty());  // top >= bottom

  EXPECT_FALSE(Rect(0, 0, 10, 10).IsEmpty());
  EXPECT_FALSE(Rect(10, 20, 100, 200).IsEmpty());
}

TEST(Rect, IsSorted) {
  EXPECT_TRUE(Rect(0, 0, 100, 100).IsSorted());
  EXPECT_TRUE(Rect(10, 20, 100, 200).IsSorted());
  EXPECT_TRUE(Rect(10, 20, 10, 20).IsSorted());  // Equal edges

  EXPECT_FALSE(Rect(100, 0, 0, 100).IsSorted());  // left > right
  EXPECT_FALSE(Rect(0, 100, 100, 0).IsSorted());  // top > bottom
  EXPECT_FALSE(Rect(100, 100, 0, 0).IsSorted());  // both inverted
}

TEST(Rect, IsFinite) {
  EXPECT_TRUE(Rect(0, 0, 100, 100).IsFinite());
  EXPECT_TRUE(Rect(-100, -100, 100, 100).IsFinite());

  float inf = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(Rect(inf, 0, 100, 100).IsFinite());
  EXPECT_FALSE(Rect(0, inf, 100, 100).IsFinite());
  EXPECT_FALSE(Rect(0, 0, inf, 100).IsFinite());
  EXPECT_FALSE(Rect(0, 0, 100, inf).IsFinite());

  float nan = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(Rect(nan, 0, 100, 100).IsFinite());
  EXPECT_FALSE(Rect(0, nan, 100, 100).IsFinite());
  EXPECT_FALSE(Rect(0, 0, nan, 100).IsFinite());
  EXPECT_FALSE(Rect(0, 0, 100, nan).IsFinite());
}

// ============================================================================
// Setter Methods Tests
// ============================================================================

TEST(Rect, SetEmpty) {
  Rect rect(10, 20, 100, 200);
  rect.SetEmpty();
  EXPECT_EQ(rect, Rect::MakeEmpty());
  EXPECT_TRUE(rect.IsEmpty());
}

TEST(Rect, SetLTRB) {
  Rect rect;
  rect.SetLTRB(10, 20, 100, 200);
  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, SetXYWH) {
  Rect rect;
  rect.SetXYWH(10, 20, 90, 180);
  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, SetX) {
  Rect rect(10, 20, 100, 200);
  rect.SetX(50);
  EXPECT_EQ(rect.Left(), 50.0f);
  EXPECT_EQ(rect.Right(), 140.0f);  // width preserved
  EXPECT_EQ(rect.Width(), 90.0f);
}

TEST(Rect, SetY) {
  Rect rect(10, 20, 100, 200);
  rect.SetY(50);
  EXPECT_EQ(rect.Top(), 50.0f);
  EXPECT_EQ(rect.Bottom(), 230.0f);  // height preserved
  EXPECT_EQ(rect.Height(), 180.0f);
}

TEST(Rect, SetLeft) {
  Rect rect(10, 20, 100, 200);
  rect.SetLeft(30);
  EXPECT_EQ(rect.Left(), 30.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
}

TEST(Rect, SetTop) {
  Rect rect(10, 20, 100, 200);
  rect.SetTop(40);
  EXPECT_EQ(rect.Top(), 40.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, SetRight) {
  Rect rect(10, 20, 100, 200);
  rect.SetRight(150);
  EXPECT_EQ(rect.Right(), 150.0f);
  EXPECT_EQ(rect.Left(), 10.0f);
}

TEST(Rect, SetBottom) {
  Rect rect(10, 20, 100, 200);
  rect.SetBottom(250);
  EXPECT_EQ(rect.Bottom(), 250.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
}

TEST(Rect, SetWH) {
  Rect rect(10, 20, 100, 200);
  rect.SetWH(50, 80);
  EXPECT_EQ(rect.Left(), 0.0f);
  EXPECT_EQ(rect.Top(), 0.0f);
  EXPECT_EQ(rect.Right(), 50.0f);
  EXPECT_EQ(rect.Bottom(), 80.0f);
}

TEST(Rect, SetWithTwoPoints) {
  Rect rect;
  // Point is a Vec4 alias, so we need to provide all 4 components
  Point p0{100, 200, 0, 0};
  Point p1{10, 20, 0, 0};

  rect.Set(p0, p1);
  // Should be sorted with min/max
  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, SetBounds) {
  // Point is a Vec4 alias, so we need to provide all 4 components
  Point pts[] = {
      {10, 20, 0, 0}, {100, 50, 0, 0}, {50, 200, 0, 0}, {5, 30, 0, 0}};

  Rect rect;
  rect.SetBounds(pts, 4);

  EXPECT_EQ(rect.Left(), 5.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, SetBoundsEmpty) {
  Rect rect(10, 20, 100, 200);
  rect.SetBounds(nullptr, 0);
  EXPECT_TRUE(rect.IsEmpty());
}

TEST(Rect, SetBoundsSinglePoint) {
  Point pts[] = {{50, 75, 0, 0}};

  Rect rect;
  rect.SetBounds(pts, 1);

  EXPECT_EQ(rect.Left(), 50.0f);
  EXPECT_EQ(rect.Top(), 75.0f);
  EXPECT_EQ(rect.Right(), 50.0f);
  EXPECT_EQ(rect.Bottom(), 75.0f);
  EXPECT_TRUE(rect.IsEmpty());
}

// ============================================================================
// Modification Methods Tests
// ============================================================================

TEST(Rect, Offset) {
  Rect rect(10, 20, 100, 200);
  rect.Offset(5, 10);

  EXPECT_EQ(rect.Left(), 15.0f);
  EXPECT_EQ(rect.Top(), 30.0f);
  EXPECT_EQ(rect.Right(), 105.0f);
  EXPECT_EQ(rect.Bottom(), 210.0f);
}

TEST(Rect, OffsetNegative) {
  Rect rect(10, 20, 100, 200);
  rect.Offset(-5, -10);

  EXPECT_EQ(rect.Left(), 5.0f);
  EXPECT_EQ(rect.Top(), 10.0f);
  EXPECT_EQ(rect.Right(), 95.0f);
  EXPECT_EQ(rect.Bottom(), 190.0f);
}

TEST(Rect, InsetSingleValue) {
  Rect rect(0, 0, 100, 100);
  rect.Inset(10);

  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 10.0f);
  EXPECT_EQ(rect.Right(), 90.0f);
  EXPECT_EQ(rect.Bottom(), 90.0f);
}

TEST(Rect, InsetTwoValues) {
  Rect rect(0, 0, 100, 100);
  rect.Inset(10, 20);

  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 90.0f);
  EXPECT_EQ(rect.Bottom(), 80.0f);
}

TEST(Rect, InsetNegative) {
  Rect rect(10, 10, 90, 90);
  rect.Inset(-5, -5);

  EXPECT_EQ(rect.Left(), 5.0f);
  EXPECT_EQ(rect.Top(), 5.0f);
  EXPECT_EQ(rect.Right(), 95.0f);
  EXPECT_EQ(rect.Bottom(), 95.0f);
}

TEST(Rect, OutsetSingleValue) {
  Rect rect(10, 10, 90, 90);
  rect.Outset(5);

  EXPECT_EQ(rect.Left(), 5.0f);
  EXPECT_EQ(rect.Top(), 5.0f);
  EXPECT_EQ(rect.Right(), 95.0f);
  EXPECT_EQ(rect.Bottom(), 95.0f);
}

TEST(Rect, OutsetTwoValues) {
  Rect rect(10, 20, 90, 80);
  rect.Outset(5, 10);

  EXPECT_EQ(rect.Left(), 5.0f);
  EXPECT_EQ(rect.Top(), 10.0f);
  EXPECT_EQ(rect.Right(), 95.0f);
  EXPECT_EQ(rect.Bottom(), 90.0f);
}

TEST(Rect, Sort) {
  Rect rect(100, 200, 10, 20);
  EXPECT_FALSE(rect.IsSorted());

  rect.Sort();

  EXPECT_TRUE(rect.IsSorted());
  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, SortAlreadySorted) {
  Rect rect(10, 20, 100, 200);
  rect.Sort();

  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, RoundOut) {
  Rect rect(10.3f, 20.7f, 100.2f, 200.8f);
  rect.RoundOut();

  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 101.0f);
  EXPECT_EQ(rect.Bottom(), 201.0f);
}

TEST(Rect, RoundIn) {
  Rect rect(10.3f, 20.7f, 100.9f, 200.2f);
  rect.RoundIn();

  EXPECT_EQ(rect.Left(), 11.0f);
  EXPECT_EQ(rect.Top(), 21.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, Round) {
  Rect rect(10.3f, 20.7f, 100.2f, 200.8f);
  rect.Round();

  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 21.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 201.0f);
}

// ============================================================================
// Factory Methods Tests
// ============================================================================

TEST(Rect, MakeEmpty) {
  Rect rect = Rect::MakeEmpty();
  EXPECT_TRUE(rect.IsEmpty());
  EXPECT_EQ(rect.Left(), 0.0f);
  EXPECT_EQ(rect.Top(), 0.0f);
  EXPECT_EQ(rect.Right(), 0.0f);
  EXPECT_EQ(rect.Bottom(), 0.0f);
}

TEST(Rect, MakeWH) {
  Rect rect = Rect::MakeWH(100, 200);
  EXPECT_EQ(rect.Left(), 0.0f);
  EXPECT_EQ(rect.Top(), 0.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
  EXPECT_EQ(rect.Width(), 100.0f);
  EXPECT_EQ(rect.Height(), 200.0f);
}

TEST(Rect, MakeLTRB) {
  Rect rect = Rect::MakeLTRB(10, 20, 100, 200);
  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, MakeXYWH) {
  Rect rect = Rect::MakeXYWH(10, 20, 90, 180);
  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, MakeSize) {
  Vec2 size{100, 200};
  Rect rect = Rect::MakeSize(size);
  EXPECT_EQ(rect.Left(), 0.0f);
  EXPECT_EQ(rect.Top(), 0.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
  EXPECT_EQ(rect.Bottom(), 200.0f);
}

TEST(Rect, MakeSorted) {
  Rect unsorted(100, 200, 10, 20);
  Rect sorted = unsorted.MakeSorted();

  EXPECT_TRUE(sorted.IsSorted());
  EXPECT_EQ(sorted.Left(), 10.0f);
  EXPECT_EQ(sorted.Top(), 20.0f);
  EXPECT_EQ(sorted.Right(), 100.0f);
  EXPECT_EQ(sorted.Bottom(), 200.0f);

  // Original should be unchanged
  EXPECT_FALSE(unsorted.IsSorted());
}

TEST(Rect, MakeOffset) {
  Rect rect(10, 20, 100, 200);
  Rect offsetRect = rect.MakeOffset(5, 10);

  EXPECT_EQ(offsetRect.Left(), 15.0f);
  EXPECT_EQ(offsetRect.Top(), 30.0f);
  EXPECT_EQ(offsetRect.Right(), 105.0f);
  EXPECT_EQ(offsetRect.Bottom(), 210.0f);

  // Original should be unchanged
  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
}

TEST(Rect, MakeInset) {
  Rect rect(0, 0, 100, 100);
  Rect insetRect = rect.MakeInset(10, 20);

  EXPECT_EQ(insetRect.Left(), 10.0f);
  EXPECT_EQ(insetRect.Top(), 20.0f);
  EXPECT_EQ(insetRect.Right(), 90.0f);
  EXPECT_EQ(insetRect.Bottom(), 80.0f);

  // Original should be unchanged
  EXPECT_EQ(rect.Left(), 0.0f);
  EXPECT_EQ(rect.Right(), 100.0f);
}

TEST(Rect, MakeOutset) {
  Rect rect(10, 20, 90, 80);
  Rect outsetRect = rect.MakeOutset(5, 10);

  EXPECT_EQ(outsetRect.Left(), 5.0f);
  EXPECT_EQ(outsetRect.Top(), 10.0f);
  EXPECT_EQ(outsetRect.Right(), 95.0f);
  EXPECT_EQ(outsetRect.Bottom(), 90.0f);

  // Original should be unchanged
  EXPECT_EQ(rect.Left(), 10.0f);
  EXPECT_EQ(rect.Top(), 20.0f);
}

// ============================================================================
// Geometric Operations Tests
// ============================================================================

TEST(Rect, Join) {
  Rect rect1(0, 0, 100, 100);
  Rect rect2(50, 50, 150, 150);

  rect1.Join(rect2);

  EXPECT_EQ(rect1.Left(), 0.0f);
  EXPECT_EQ(rect1.Top(), 0.0f);
  EXPECT_EQ(rect1.Right(), 150.0f);
  EXPECT_EQ(rect1.Bottom(), 150.0f);
}

TEST(Rect, JoinDisjoint) {
  Rect rect1(0, 0, 100, 100);
  Rect rect2(200, 200, 300, 300);

  rect1.Join(rect2);

  EXPECT_EQ(rect1.Left(), 0.0f);
  EXPECT_EQ(rect1.Top(), 0.0f);
  EXPECT_EQ(rect1.Right(), 300.0f);
  EXPECT_EQ(rect1.Bottom(), 300.0f);
}

TEST(Rect, JoinEmpty) {
  Rect rect1(10, 20, 100, 200);
  Rect rect2 = Rect::MakeEmpty();

  rect1.Join(rect2);

  EXPECT_EQ(rect1.Left(), 10.0f);
  EXPECT_EQ(rect1.Top(), 20.0f);
  EXPECT_EQ(rect1.Right(), 100.0f);
  EXPECT_EQ(rect1.Bottom(), 200.0f);
}

TEST(Rect, JoinToEmpty) {
  Rect rect1 = Rect::MakeEmpty();
  Rect rect2(10, 20, 100, 200);

  rect1.Join(rect2);

  EXPECT_EQ(rect1, rect2);
}

TEST(Rect, Intersect) {
  Rect rect1 = Rect::MakeLTRB(0, 0, 100, 100);
  Rect rect2 = Rect::MakeLTRB(200, 200, 300, 300);
  Rect rect3 = Rect::MakeLTRB(150, 250, 350, 260);

  EXPECT_FALSE(rect1.Intersect(rect2));
  EXPECT_EQ(Rect::MakeLTRB(0, 0, 100, 100), rect1);

  EXPECT_FALSE(rect1.Intersect(Rect::MakeEmpty()));
  EXPECT_EQ(Rect::MakeLTRB(0, 0, 100, 100), rect1);

  EXPECT_TRUE(rect2.Intersect(rect3));
  EXPECT_EQ(Rect::MakeLTRB(200, 250, 300, 260), rect2);
}

TEST(Rect, IntersectOverlapping) {
  Rect rect1(0, 0, 100, 100);
  Rect rect2(50, 50, 150, 150);

  EXPECT_TRUE(rect1.Intersect(rect2));

  EXPECT_EQ(rect1.Left(), 50.0f);
  EXPECT_EQ(rect1.Top(), 50.0f);
  EXPECT_EQ(rect1.Right(), 100.0f);
  EXPECT_EQ(rect1.Bottom(), 100.0f);
}

TEST(Rect, IntersectContained) {
  Rect rect1(0, 0, 100, 100);
  Rect rect2(25, 25, 75, 75);

  EXPECT_TRUE(rect1.Intersect(rect2));

  EXPECT_EQ(rect1, rect2);
}

TEST(Rect, IntersectStatic) {
  Rect rect1(0, 0, 100, 100);
  Rect rect2(50, 50, 150, 150);

  EXPECT_TRUE(Rect::Intersect(rect1, rect2));

  // Original rects should be unchanged
  EXPECT_EQ(rect1.Right(), 100.0f);
  EXPECT_EQ(rect2.Left(), 50.0f);
}

TEST(Rect, IntersectStaticNoIntersection) {
  Rect rect1(0, 0, 100, 100);
  Rect rect2(200, 200, 300, 300);

  EXPECT_FALSE(Rect::Intersect(rect1, rect2));
}

TEST(Rect, ContainsPoint) {
  Rect rect(0, 0, 100, 100);

  EXPECT_TRUE(rect.Contains(50, 50));
  EXPECT_TRUE(rect.Contains(0, 0));
  EXPECT_TRUE(rect.Contains(99, 99));

  EXPECT_FALSE(
      rect.Contains(100, 100));  // Edge case: right/bottom not included
  EXPECT_FALSE(rect.Contains(100, 50));
  EXPECT_FALSE(rect.Contains(50, 100));
  EXPECT_FALSE(rect.Contains(-1, 50));
  EXPECT_FALSE(rect.Contains(50, -1));
  EXPECT_FALSE(rect.Contains(150, 50));
}

TEST(Rect, ContainsRect) {
  Rect rect1(0, 0, 100, 100);
  Rect rect2(25, 25, 75, 75);
  Rect rect3(50, 50, 150, 150);
  Rect rect4(200, 200, 300, 300);

  EXPECT_TRUE(rect1.Contains(rect2));   // Fully contained
  EXPECT_FALSE(rect1.Contains(rect3));  // Partially overlapping
  EXPECT_FALSE(rect1.Contains(rect4));  // Disjoint
  EXPECT_TRUE(rect1.Contains(rect1));   // Can contain itself (same boundaries)
}

TEST(Rect, ContainsEmptyRect) {
  Rect rect(0, 0, 100, 100);
  Rect empty = Rect::MakeEmpty();

  EXPECT_FALSE(rect.Contains(empty));
}

TEST(Rect, EmptyRectContains) {
  Rect empty = Rect::MakeEmpty();
  Rect rect(25, 25, 75, 75);

  EXPECT_FALSE(empty.Contains(rect));
  EXPECT_FALSE(empty.Contains(50, 50));
}

TEST(Rect, ToQuad) {
  Rect rect(10, 20, 100, 200);
  Point quad[4];

  rect.ToQuad(quad);

  // Top-left
  EXPECT_EQ(quad[0].x, 10.0f);
  EXPECT_EQ(quad[0].y, 20.0f);

  // Top-right
  EXPECT_EQ(quad[1].x, 100.0f);
  EXPECT_EQ(quad[1].y, 20.0f);

  // Bottom-right
  EXPECT_EQ(quad[2].x, 100.0f);
  EXPECT_EQ(quad[2].y, 200.0f);

  // Bottom-left
  EXPECT_EQ(quad[3].x, 10.0f);
  EXPECT_EQ(quad[3].y, 200.0f);
}

// ============================================================================
// Static Utility Methods Tests
// ============================================================================

TEST(Rect, HalfWidth) {
  Rect rect(0, 0, 100, 100);
  EXPECT_FLOAT_EQ(Rect::HalfWidth(rect), 50.0f);
}

TEST(Rect, HalfWidthNonZeroOrigin) {
  Rect rect(10, 20, 110, 120);
  EXPECT_FLOAT_EQ(Rect::HalfWidth(rect), 50.0f);
}

TEST(Rect, HalfHeight) {
  Rect rect(0, 0, 100, 200);
  EXPECT_FLOAT_EQ(Rect::HalfHeight(rect), 100.0f);
}

TEST(Rect, HalfHeightNonZeroOrigin) {
  Rect rect(10, 20, 110, 220);
  EXPECT_FLOAT_EQ(Rect::HalfHeight(rect), 100.0f);
}
