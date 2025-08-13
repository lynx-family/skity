// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <skity/geometry/rect.hpp>
#include <skity/geometry/rrect.hpp>
#include <skity/geometry/scalar.hpp>
#include <skity/geometry/vector.hpp>

using namespace skity;

constexpr float kWidth = 100.f;
constexpr float kHeight = 100.f;

TEST(RRect, BasicAPI) {
  Vec2 zero_pt{0.f, 0.f};
  RRect empty;

  empty.SetEmpty();
  EXPECT_EQ(empty.GetType(), RRect::Type::kEmpty);
  EXPECT_TRUE(empty.GetRect().IsEmpty());

  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(empty.Radii(static_cast<RRect::Corner>(i)), zero_pt);
  }

  auto rect = Rect::MakeWH(kWidth, kHeight);

  RRect rrect1;
  rrect1.SetRect(rect);

  EXPECT_EQ(rrect1.GetType(), RRect::Type::kRect);
  EXPECT_EQ(rrect1.GetRect(), rect);

  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(rrect1.Radii(static_cast<RRect::Corner>(i)), zero_pt);
  }

  RRect rrect1_2;
  std::array<Vec2, 4> rrect1_2_radii = {
      Vec2{0.f, 0.f},
      Vec2{0.f, 0.f},
      Vec2{0.f, 0.f},
      Vec2{0.f, 0.f},
  };

  rrect1_2.SetRectRadii(rect, rrect1_2_radii.data());
  EXPECT_EQ(rrect1_2, rrect1);
  EXPECT_EQ(rrect1_2.GetType(), rrect1.GetType());

  Vec2 half_point{SkityFloatHalf(kWidth), SkityFloatHalf(kHeight)};

  RRect rrect2;
  rrect2.SetOval(rect);

  EXPECT_EQ(rrect2.GetType(), RRect::Type::kOval);
  EXPECT_EQ(rrect2.GetRect(), rect);
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(rrect2.Radii(static_cast<RRect::Corner>(i)), half_point);
  }

  RRect rrect2_2;
  std::array<Vec2, 4> rrect2_2_radii = {
      half_point,
      half_point,
      half_point,
      half_point,
  };

  rrect2_2.SetRectRadii(rect, rrect2_2_radii.data());
  EXPECT_EQ(rrect2_2, rrect2);
  EXPECT_EQ(rrect2_2.GetType(), rrect2.GetType());

  Vec2 p{5.f, 5.f};
  RRect rrect3;

  rrect3.SetRectXY(rect, p.x, p.y);

  EXPECT_EQ(rrect3.GetType(), RRect::Type::kSimple);
  EXPECT_EQ(rrect3.GetRect(), rect);
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(rrect3.Radii(static_cast<RRect::Corner>(i)), p);
  }

  RRect rrect3_2;
  std::array<Vec2, 4> rrect3_2_radii = {
      p,
      p,
      p,
      p,
  };

  rrect3_2.SetRectRadii(rect, rrect3_2_radii.data());
  EXPECT_EQ(rrect3_2, rrect3);
  EXPECT_EQ(rrect3_2.GetType(), rrect3.GetType());
}

TEST(RRect, DegenerateToRect) {
  Rect r;

  RRect empty;
  empty.SetEmpty();

  EXPECT_EQ(empty.GetType(), RRect::Type::kEmpty);

  r = empty.GetRect();
  EXPECT_TRUE(r.Left() == 0 && r.Top() == 0 && r.Right() == 0 &&
              r.Bottom() == 0);

  auto rect = Rect::MakeWH(kWidth, kHeight);
  RRect rrect1;
  rrect1.SetRectXY(rect, 0.f, 0.f);
  EXPECT_EQ(rrect1.GetType(), RRect::Type::kRect);
  r = rrect1.GetRect();
  EXPECT_EQ(r, rect);

  std::array<Vec2, 4> radii = {
      Vec2{0.f, 0.f},
      Vec2{0.f, 0.f},
      Vec2{0.f, 0.f},
      Vec2{0.f, 0.f},
  };

  RRect rrect2;
  rrect2.SetRectRadii(rect, radii.data());
  EXPECT_EQ(rrect2.GetType(), RRect::Type::kRect);
  r = rrect2.GetRect();
  EXPECT_EQ(r, rect);

  std::array<Vec2, 4> radii2 = {
      Vec2{0.f, 0.f},
      Vec2{20.f, 20.f},
      Vec2{50.f, 50.f},
      Vec2{20.f, 50.f},
  };

  RRect rrect3;

  rrect3.SetRectRadii(rect, radii2.data());
  EXPECT_EQ(rrect3.GetType(), RRect::Type::kComplex);
}

TEST(RRect, DegenerateToOval) {
  Rect oval;
  Rect rect = Rect::MakeWH(kWidth, kHeight);

  RRect rrect;

  rrect.SetRectXY(rect, SkityFloatHalf(kWidth), SkityFloatHalf(kHeight));

  EXPECT_EQ(rrect.GetType(), RRect::Type::kOval);
  oval = rrect.GetRect();
  EXPECT_EQ(oval, rect);
}

TEST(RRect, General) {
  auto rect = Rect::MakeWH(kWidth, kHeight);

  RRect rrect;
  rrect.SetRectXY(rect, 20, 20);

  EXPECT_EQ(rrect.GetType(), RRect::Type::kSimple);

  std::array<Vec2, 4> radii = {
      Vec2{0.f, 0.f},
      Vec2{20.f, 20.f},
      Vec2{50.f, 50.f},
      Vec2{20.f, 50.f},
  };

  RRect rrect2;
  rrect2.SetRectRadii(rect, radii.data());

  EXPECT_EQ(rrect2.GetType(), RRect::Type::kComplex);
}

TEST(RRect, TestRobustness) {
  auto rect = Rect::MakeWH(kWidth, kHeight);

  RRect rrect;

  rrect.SetRectXY(rect, FloatInfinity, FloatInfinity);
  EXPECT_EQ(rrect.GetType(), RRect::Type::kRect);

  rrect.SetRectXY(rect, kWidth, kHeight);
  EXPECT_EQ(rrect.GetType(), RRect::Type::kOval);

  std::array<Vec2, 4> radii = {
      Vec2{50.f, 100.f},
      Vec2{100.f, 50.f},
      Vec2{50.f, 100.f},
      Vec2{100.f, 50.f},
  };

  RRect rrect1;
  rrect1.SetRectRadii(rect, radii.data());
  EXPECT_EQ(rrect1.GetType(), RRect::Type::kComplex);

  auto p = rrect1.Radii(RRect::Corner::kUpperLeft);
  EXPECT_TRUE(FloatNearlyZero(p.x - 33.333333f));
  EXPECT_TRUE(FloatNearlyZero(p.y - 66.666666f));

  RRect rrect2;
  rrect2.SetRectXY(rect, -10, -20);

  EXPECT_EQ(rrect2.GetType(), RRect::Type::kRect);

  auto p2 = rrect2.Radii(RRect::Corner::kUpperLeft);
  EXPECT_EQ(p2.x, 0.f);
  EXPECT_EQ(p2.y, 0.f);
}

static void test_direction(const RRect& rrect, float init_x, int32_t step_x,
                           float init_y, int32_t step_y, int32_t num_steps,
                           const bool* contains) {
  float x = init_x;
  float y = init_y;

  for (int32_t i = 0; i < num_steps; i++) {
    auto test =
        Rect::MakeXYWH(x, y, step_x ? static_cast<float>(step_x) : Float1,
                       step_y ? static_cast<float>(step_y) : Float1);

    test.Sort();

    EXPECT_EQ(rrect.Contains(test), contains[i])
        << "rrect: "
        << "[ " << rrect.GetRect().Left() << ", " << rrect.GetRect().Top()
        << ", " << rrect.GetRect().Right() << ", " << rrect.GetRect().Bottom()
        << " ]"
        << " test: "
        << "[ " << test.Left() << ", " << test.Top() << ", " << test.Right()
        << ", " << test.Bottom() << " ]";

    x += step_x;
    y += step_y;
  }
}

TEST(RRect, TestContains) {
  constexpr int32_t kNumRRects = 4;
  const Vec2 kRadii[kNumRRects][4] = {
      // rect ----
      {Vec2{0.f, 0.f}, Vec2{0.f, 0.f}, Vec2{0.f, 0.f}, Vec2{0.f, 0.f}},
      // circle ------
      {Vec2{20.f, 20.f}, Vec2{20.f, 20.f}, Vec2{20.f, 20.f}, Vec2{20.f, 20.f}},
      // simple ------
      {Vec2{10.f, 10.f}, Vec2{10.f, 10.f}, Vec2{10.f, 10.f}, Vec2{10.f, 10.f}},
      // complex ------
      {Vec2{0.f, 0.f}, Vec2{20.f, 20.f}, Vec2{10.f, 10.f}, Vec2{30.f, 30.f}},
  };

  std::array<RRect, kNumRRects> rrects;

  for (size_t i = 0; i < rrects.size(); i++) {
    rrects[i].SetRectRadii(Rect::MakeWH(40, 40), kRadii[i]);
  }

  std::array<Rect, 8> easy_outs = {
      Rect::MakeLTRB(-5, -5, 5, 5),    //
      Rect::MakeLTRB(15, -5, 20, 5),   //
      Rect::MakeLTRB(35, -5, 45, 5),   //
      Rect::MakeLTRB(35, 15, 45, 20),  //
      Rect::MakeLTRB(35, 45, 35, 45),  //
      Rect::MakeLTRB(15, 35, 20, 45),  //
      Rect::MakeLTRB(-5, 35, 5, 45),   //
      Rect::MakeLTRB(-5, 15, 5, 20),   //
  };

  for (int32_t i = 0; i < kNumRRects; i++) {
    for (size_t j = 0; j < easy_outs.size(); j++) {
      EXPECT_FALSE(rrects[i].Contains(easy_outs[j]));
    }
  }

  static const int kNumSteps = 15;
  bool answers[kNumRRects][8][kNumSteps] = {
      // all the test rects are inside the degenerate rrect
      {
          // rect
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      },
      // for the circle we expect 6 blocks to be out on the
      // corners (then the rest in) and only the first block
      // out on the vertical and horizontal axes (then
      // the rest in)
      {
          // circle
          {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      },
      // for the simple round rect we expect 3 out on
      // the corners (then the rest in) and no blocks out
      // on the vertical and horizontal axes
      {
          // simple RR
          {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      },
      // for the complex case the answer is different for each direction
      {
          // complex RR
          // all in for NW (rect) corner (same as rect case)
          {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          // only first block out for N (same as circle case)
          {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          // first 6 blocks out for NE (same as circle case)
          {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          // only first block out for E (same as circle case)
          {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          // first 3 blocks out for SE (same as simple case)
          {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          // first two blocks out for S
          {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          // first 9 blocks out for SW
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1},
          // first two blocks out for W (same as S)
          {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      }};

  for (int i = 0; i < kNumRRects; ++i) {
    test_direction(rrects[i], 0, 1, 0, 1, kNumSteps, answers[i][0]);
    test_direction(rrects[i], 19.5f, 0, 0, 1, kNumSteps, answers[i][1]);
    test_direction(rrects[i], 40, -1, 0, 1, kNumSteps, answers[i][2]);
    test_direction(rrects[i], 40, -1, 19.5f, 0, kNumSteps, answers[i][3]);
    test_direction(rrects[i], 40, -1, 40, -1, kNumSteps, answers[i][4]);
    test_direction(rrects[i], 19.5f, 0, 40, -1, kNumSteps, answers[i][5]);
    test_direction(rrects[i], 0, 1, 40, -1, kNumSteps, answers[i][6]);
    test_direction(rrects[i], 0, 1, 19.5f, 0, kNumSteps, answers[i][7]);
  }
}

TEST(RRect, TestEmpty) {
  std::array<Rect, 3> rects_out_of_order = {
      Rect::MakeLTRB(100, 0, 0, 100),
      Rect::MakeLTRB(0, 100, 100, 0),
      Rect::MakeLTRB(100, 100, 0, 0),
  };

  std::array<Rect, 4> rects_empty = {
      Rect::MakeLTRB(100, 100, 100, 200),
      Rect::MakeLTRB(100, 100, 200, 100),
      Rect::MakeLTRB(100, 100, 100, 100),
      Rect::MakeEmpty(),
  };

  std::array<Vec2, 4> radii = {
      Vec2{0.f, 1.f},
      Vec2{2.f, 3.f},
      Vec2{4.f, 5.f},
      Vec2{6.f, 7.f},
  };

  for (size_t i = 0; i < rects_out_of_order.size(); i++) {
    RRect rrect;

    rrect.SetRect(rects_out_of_order[i]);
    EXPECT_TRUE(!rrect.IsEmpty());
    EXPECT_EQ(rrect.GetRect(), rects_out_of_order[i].MakeSorted());

    rrect.SetOval(rects_out_of_order[i]);
    EXPECT_TRUE(!rrect.IsEmpty());
    EXPECT_EQ(rrect.GetRect(), rects_out_of_order[i].MakeSorted());

    rrect.SetRectXY(rects_out_of_order[i], 1.f, 2.f);
    EXPECT_TRUE(!rrect.IsEmpty());
    EXPECT_EQ(rrect.GetRect(), rects_out_of_order[i].MakeSorted());

    rrect.SetRectRadii(rects_out_of_order[i], radii.data());
    EXPECT_TRUE(!rrect.IsEmpty());
    EXPECT_EQ(rrect.GetRect(), rects_out_of_order[i].MakeSorted());
  }

  for (size_t i = 0; i < rects_empty.size(); i++) {
    RRect rrect;

    rrect.SetRect(rects_empty[i]);
    EXPECT_TRUE(rrect.IsEmpty());
    EXPECT_EQ(rrect.GetRect(), rects_empty[i]);

    rrect.SetOval(rects_empty[i]);
    EXPECT_TRUE(rrect.IsEmpty());
    EXPECT_EQ(rrect.GetRect(), rects_empty[i]);

    rrect.SetRectXY(rects_empty[i], 1.f, 2.f);
    EXPECT_TRUE(rrect.IsEmpty());
    EXPECT_EQ(rrect.GetRect(), rects_empty[i]);

    rrect.SetRectRadii(rects_empty[i], radii.data());
    EXPECT_TRUE(rrect.IsEmpty());
    EXPECT_EQ(rrect.GetRect(), rects_empty[i]);
  }

  RRect rrect;
  rrect.SetRect(Rect::MakeLTRB(FloatNaN, 10.f, 10.f, 20.f));
  EXPECT_EQ(rrect.GetRect(), Rect::MakeEmpty());

  rrect.SetRect(Rect::MakeLTRB(0.f, 10.f, 20.f, FloatInfinity));
  EXPECT_EQ(rrect.GetRect(), Rect::MakeEmpty());
}

TEST(RRect, TestInset) {
  RRect rrect1, rrect2;

  Rect rect = Rect::MakeLTRB(0.f, 0.f, 100.f, 100.f);

  rrect1.SetRect(rect);
  rrect1.Inset(-20.f, -20.f, &rrect2);
  EXPECT_TRUE(rrect2.IsRect());

  rrect1.Inset(20.f, 20.f, &rrect2);
  EXPECT_TRUE(rrect2.IsRect());

  rrect1.Inset(rect.Width() / 2.f, rect.Height() / 2.f, &rrect2);
  EXPECT_TRUE(rrect2.IsEmpty());

  rrect1.SetRectXY(rect, 20, 20);
  rrect1.Inset(19, 19, &rrect2);
  EXPECT_TRUE(rrect2.IsSimple());

  rrect1.Inset(20, 20, &rrect2);
  EXPECT_TRUE(rrect2.IsRect());

  rrect1.Inset(FloatInfinity, FloatInfinity, &rrect2);
  EXPECT_TRUE(rrect2.IsEmpty());
}
