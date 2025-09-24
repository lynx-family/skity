// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "skity/geometry/vector.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>

using namespace skity;

template <typename T>
constexpr size_t Dimension() {
  if constexpr (std::is_same_v<T, Vec2>) {
    return 2;
  } else if constexpr (std::is_same_v<T, Vec3>) {
    return 3;
  } else if constexpr (std::is_same_v<T, Vec4>) {
    return 4;
  } else {
    static_assert([] { return false; }(), "Unknown vector type");
  }
}

template <typename T>
static void EXPECT_VEC_EQ(const T& a, const T& b) {
  for (size_t i = 0; i < Dimension<T>(); i++) {
    EXPECT_FLOAT_EQ(a[i], b[i]);
  }
}

TEST(Vec2Test, Constructor) {
  Vec2 v;
  ASSERT_EQ(v.x, 0);
  ASSERT_EQ(v.y, 0);

  Vec2 v2(1.2, 2.3);
  ASSERT_FLOAT_EQ(v2.x, 1.2);
  ASSERT_FLOAT_EQ(v2.y, 2.3);

  Vec4 src(3, 4, 5, 6);
  Vec2 v3(src);
  ASSERT_EQ(v3.x, 3);
  ASSERT_EQ(v3.y, 4);

  Vec2 v4{6};
  ASSERT_EQ(v4.x, 6);
  ASSERT_EQ(v4.y, 6);
}

TEST(Vec2Test, Equals) {
  Vec2 a(1, 2);
  Vec2 b(1, 2);

  ASSERT_TRUE(a == b);
  ASSERT_FALSE(a != b);

  Vec2 c(1, 3);
  ASSERT_TRUE(a != c);
  ASSERT_FALSE(a == c);
}

TEST(Vec2Test, Operators) {
  Vec2 a(1, 2);
  Vec2 b(3, 4);

  ASSERT_EQ(a + b, Vec2(4, 6));
  ASSERT_EQ(a - b, Vec2(-2, -2));
  ASSERT_EQ(a * 2, Vec2(2, 4));
  ASSERT_EQ(2 * a, Vec2(2, 4));
  ASSERT_EQ(a / 2, Vec2(0.5f, 1));
  ASSERT_EQ(2 / a, Vec2(2, 1));

  Vec2 c = a;
  c += b;
  ASSERT_EQ(c, Vec2(4, 6));

  c -= a;
  ASSERT_EQ(c, Vec2(3, 4));

  c *= 2;
  ASSERT_EQ(c, Vec2(6, 8));

  c *= Vec2{2, 3};
  ASSERT_EQ(c, Vec2(12, 24));

  c /= 2;
  ASSERT_EQ(c, Vec2(6, 12));
}

TEST(Vec2Test, DotCross) {
  Vec2 a(1, 2);
  Vec2 b(3, 4);

  ASSERT_EQ(Vec2::Dot(a, b), 11);
  ASSERT_EQ(a.Dot(b), 11);
  ASSERT_EQ(Vec2::Cross(a, b), -2);
  ASSERT_EQ(a.Cross(b), -2);
}

TEST(Vec2Test, Length) {
  Vec2 v(3, 4);
  ASSERT_EQ(v.LengthSquared(), 25);
  ASSERT_EQ(v.Length(), 5);
}

TEST(Vec2Test, Normalize) {
  Vec2 v(3, 4);
  Vec2 norm = v.Normalize();
  ASSERT_NEAR(norm.Length(), 1.0f, 1e-6);
  EXPECT_VEC_EQ(norm, Vec2(3.0f / 5.0f, 4.0f / 5.0f));

  Vec2 zero(0, 0);
  ASSERT_EQ(zero.Normalize(), Vec2(0, 0));
}

TEST(Vec2Test, MinMax) {
  Vec2 a(1, 5);
  Vec2 b(3, 2);

  ASSERT_EQ(Vec2::Min(a, b), Vec2(1, 2));
  ASSERT_EQ(Vec2::Max(a, b), Vec2(3, 5));
}

TEST(Vec2Test, SqrtRoundAbs) {
  Vec2 v(4, 9);
  ASSERT_EQ(Vec2::Sqrt(v), Vec2(2, 3));

  Vec2 v2(1.2f, 3.7f);
  ASSERT_EQ(Vec2::Round(v2), Vec2(1, 4));

  Vec2 v3(-2, -3);
  ASSERT_EQ(Vec2::Abs(v3), Vec2(2, 3));
}

TEST(Vec2Test, VecOperators) {
  Vec2 a(2, 4);
  Vec2 b(3, 2);
  ASSERT_EQ(a * b, Vec2(6, 8));
  EXPECT_VEC_EQ(a / b, Vec2(2.0f / 3.0f, 2.0f));
}

TEST(Vec2Test, IndexOperator) {
  Vec2 v(1, 2);
  ASSERT_EQ(v[0], 1);
  ASSERT_EQ(v[1], 2);
  v[0] = 3;
  v[1] = 4;
  ASSERT_EQ(v, Vec2(3, 4));
}

TEST(Vec2Test, UnaryMinus) {
  Vec2 v(1, -2);
  ASSERT_EQ(-v, Vec2(-1, 2));
}

TEST(Vec3Test, Constructor) {
  Vec3 v;
  ASSERT_EQ(v.x, 0);
  ASSERT_EQ(v.y, 0);
  ASSERT_EQ(v.z, 0);

  Vec3 v2(1, 2, 3);
  ASSERT_EQ(v2.x, 1);
  ASSERT_EQ(v2.y, 2);
  ASSERT_EQ(v2.z, 3);

  Vec3 v3(6.f);
  ASSERT_EQ(v3.x, 6.f);
  ASSERT_EQ(v3.y, 6.f);
  ASSERT_EQ(v3.z, 6.f);
}

TEST(Vec3Test, Equals) {
  Vec3 a(1, 2, 3);
  Vec3 b(1, 2, 3);

  ASSERT_TRUE(a == b);
  ASSERT_FALSE(a != b);

  Vec3 c(1, 3, 2);
  ASSERT_TRUE(a != c);
  ASSERT_FALSE(a == c);
}

TEST(Vec3Test, Operators) {
  Vec3 a(1, 2, 3);
  Vec3 b(4, 5, 6);

  ASSERT_EQ(a + b, Vec3(5, 7, 9));
  ASSERT_EQ(a - b, Vec3(-3, -3, -3));
  ASSERT_EQ(a * 2, Vec3(2, 4, 6));
  ASSERT_EQ(2 * a, Vec3(2, 4, 6));
  ASSERT_EQ(a / 2.f, Vec3(0.5f, 1.f, 1.5f));
  ASSERT_EQ(2.f / a, Vec3(2.f, 1.f, 2 / 3.f));

  Vec3 c = a;
  c += b;
  ASSERT_EQ(c, Vec3(5, 7, 9));

  c -= b;
  ASSERT_EQ(c, Vec3(1, 2, 3));

  c *= 3;
  ASSERT_EQ(c, Vec3(3, 6, 9));

  c *= Vec3{2, 3, 4};
  ASSERT_EQ(c, Vec3(6, 18, 36));

  c /= 3.f;
  ASSERT_EQ(c, Vec3(2, 6, 12));
}

TEST(Vec3Test, DotCross) {
  Vec3 a(2, 3, 4);
  Vec3 b(5, 1, 3);

  ASSERT_EQ(Vec3::Dot(a, b), 25);
  ASSERT_EQ(Vec3::Cross(a, b), Vec3(5, 14, -13));
}

TEST(Vec3Test, Length) {
  Vec3 v(3, 4, 5);
  ASSERT_EQ(v.LengthSquared(), 50);
  ASSERT_FLOAT_EQ(v.Length(), std::sqrt(50.0f));
}

TEST(Vec3Test, Normalize) {
  Vec3 v(2, 3, 4);
  Vec3 norm = v.Normalize();
  ASSERT_NEAR(norm.Length(), 1.0f, 1e-6);

  EXPECT_VEC_EQ(norm, Vec3(2.0f / std::sqrt(29.f), 3.0f / std::sqrt(29.f),
                           4.0f / std::sqrt(29.f)));

  Vec3 zero(0, 0, 0);
  ASSERT_EQ(zero.Normalize(), Vec3(0, 0, 0));
}

TEST(Vec3Test, MinMax) {
  Vec3 a(1, 5, 3);
  Vec3 b(3, 2, 4);

  ASSERT_EQ(Vec3::Min(a, b), Vec3(1, 2, 3));
  ASSERT_EQ(Vec3::Max(a, b), Vec3(3, 5, 4));
}

TEST(Vec3Test, VecOperators) {
  Vec3 a(2, 3, 4);
  Vec3 b(5, 6, 7);

  ASSERT_EQ(a * b, Vec3(10, 18, 28));
  ASSERT_EQ(a / b, Vec3(2.f / 5.f, 3.f / 6.f, 4.f / 7.f));
}

TEST(Vec3Test, IndexOperator) {
  Vec3 v(1, 2, 3);
  ASSERT_EQ(v[0], 1);
  ASSERT_EQ(v[1], 2);
  ASSERT_EQ(v[2], 3);
}

TEST(Vec3Test, UnaryMinus) {
  Vec3 v(1, -2, 3);
  ASSERT_EQ(-v, Vec3(-1, 2, -3));
}

TEST(Vec4Test, Constructor) {
  Vec4 v;
  ASSERT_EQ(v.x, 0);
  ASSERT_EQ(v.y, 0);
  ASSERT_EQ(v.z, 0);
  ASSERT_EQ(v.w, 0);

  Vec4 v2(1, 2, 3, 4);
  ASSERT_EQ(v2.x, 1);
  ASSERT_EQ(v2.y, 2);
  ASSERT_EQ(v2.z, 3);
  ASSERT_EQ(v2.w, 4);

  Vec4 v3{5.f};
  ASSERT_EQ(v3.x, 5.f);
  ASSERT_EQ(v3.y, 5.f);
  ASSERT_EQ(v3.z, 5.f);
  ASSERT_EQ(v3.w, 5.f);

  Vec2 xy{1.f, 2.f};

  Vec4 v4{xy, 3.f, 4.f};
  ASSERT_EQ(v4.x, 1.f);
  ASSERT_EQ(v4.y, 2.f);
  ASSERT_EQ(v4.z, 3.f);
  ASSERT_EQ(v4.w, 4.f);

  Vec2 zw{5.f, 6.f};
  Vec4 v5{xy, zw};
  ASSERT_EQ(v5.x, 1.f);
  ASSERT_EQ(v5.y, 2.f);
  ASSERT_EQ(v5.z, 5.f);
  ASSERT_EQ(v5.w, 6.f);

  Vec3 xyz{3.f, 4.f, 5.f};
  Vec4 v6{xyz, 6.f};
  ASSERT_EQ(v6.x, 3.f);
  ASSERT_EQ(v6.y, 4.f);
  ASSERT_EQ(v6.z, 5.f);
  ASSERT_EQ(v6.w, 6.f);
}

TEST(Vec4Test, Equals) {
  Vec4 a(1, 2, 3, 4);
  Vec4 b(1, 2, 3, 4);

  ASSERT_TRUE(a == b);
  ASSERT_FALSE(a != b);

  Vec4 c(1, 3, 2, 4);
  ASSERT_TRUE(a != c);
  ASSERT_FALSE(a == c);
}

TEST(Vec4Test, Operators) {
  Vec4 a(1, 2, 3, 4);
  Vec4 b(5, 6, 7, 8);

  ASSERT_EQ(a + b, Vec4(6, 8, 10, 12));
  ASSERT_EQ(a - b, Vec4(-4, -4, -4, -4));
  ASSERT_EQ(a * 2, Vec4(2, 4, 6, 8));
  ASSERT_EQ(2 * a, Vec4(2, 4, 6, 8));
  ASSERT_EQ(a / 2, Vec4(0.5f, 1, 1.5f, 2));
  ASSERT_EQ(2 / a, Vec4(2.f, 1.f, 2 / 3.f, 0.5f));

  Vec4 c = a;
  c += b;
  ASSERT_EQ(c, Vec4(6, 8, 10, 12));

  c -= b;
  ASSERT_EQ(c, Vec4(1, 2, 3, 4));

  c *= 3;
  ASSERT_EQ(c, Vec4(3, 6, 9, 12));

  c /= 3.f;
  ASSERT_EQ(c, Vec4(1, 2, 3, 4));

  c *= Vec4{2, 3, 4, 5};
  ASSERT_EQ(c, Vec4(2, 6, 12, 20));
}

TEST(Vec4Test, Dot) {
  Vec4 a(1, 2, 3, 4);
  Vec4 b(5, 6, 7, 8);
  ASSERT_EQ(Vec4::Dot(a, b), 1 * 5 + 2 * 6 + 3 * 7 + 4 * 8);
}

TEST(Vec4Test, ColorChannels) {
  Vec4 v(1, 2, 3, 4);
  ASSERT_EQ(v.r, 1);
  ASSERT_EQ(v.g, 2);
  ASSERT_EQ(v.b, 3);
  ASSERT_EQ(v.a, 4);
}

TEST(Vec4Test, VecOperators) {
  Vec4 a(2, 4, 6, 8);
  Vec4 b(2, 2, 2, 2);
  ASSERT_EQ(a * b, Vec4(4, 8, 12, 16));
  ASSERT_EQ(a / b, Vec4(1, 2, 3, 4));
}

TEST(Vec4Test, Length) {
  Vec4 v(3, 4, 5, 6);
  ASSERT_EQ(v.LengthSquared(), 86);
  ASSERT_FLOAT_EQ(v.Length(), std::sqrt(86.0f));
}

TEST(Vec4Test, Normalize) {
  Vec4 v(1, 2, 3, 4);
  Vec4 norm = v.Normalize();
  ASSERT_NEAR(norm.Length(), 1.0f, 1e-6);
  EXPECT_VEC_EQ(norm, {1.f / std::sqrt(30.f), 2.f / std::sqrt(30.f),
                       3.f / std::sqrt(30.f), 4.f / std::sqrt(30.f)});

  Vec4 zero(0, 0, 0, 0);
  ASSERT_EQ(zero.Normalize(), Vec4(0, 0, 0, 0));
}

TEST(Vec4Test, MinMax) {
  Vec4 a(1, 3, 5, 7);
  Vec4 b(2, 2, 6, 6);

  ASSERT_EQ(Vec4::Min(a, b), Vec4(1, 2, 5, 6));
  ASSERT_EQ(Vec4::Max(a, b), Vec4(2, 3, 6, 7));
}

TEST(Vec4Test, IndexOperator) {
  Vec4 v(1, 2, 3, 4);
  ASSERT_EQ(v[0], 1);
  ASSERT_EQ(v[1], 2);
  ASSERT_EQ(v[2], 3);
  ASSERT_EQ(v[3], 4);

  v[0] = 5;
  v[1] = 6;
  v[2] = 7;
  v[3] = 8;
  ASSERT_EQ(v[0], 5);
  ASSERT_EQ(v[1], 6);
  ASSERT_EQ(v[2], 7);
  ASSERT_EQ(v[3], 8);
}

TEST(Vec4Test, UnaryMinus) {
  Vec4 v(1, -2, 3, -4);
  ASSERT_EQ(-v, Vec4(-1, 2, -3, 4));
}

TEST(Vec4Test, XYZW) {
  Vec4 v(1, -2, 3, -4);
  ASSERT_EQ(v.xy(), Vec2(1, -2));
  ASSERT_EQ(v.zw(), Vec2(3, -4));
}
