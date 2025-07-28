// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/geometry/matrix.hpp>
#include <skity/geometry/quaternion.hpp>
#include <skity/geometry/rect.hpp>

#include "gtest/gtest.h"
#include "test/ut/common.hpp"

TEST(Matrix, RectStaysRect) {
  auto m = skity::Matrix();
  EXPECT_TRUE(m.RectStaysRect());
  m = skity::Matrix::Translate(50.0, 100.0);
  EXPECT_TRUE(m.RectStaysRect());
  m = skity::Matrix::Scale(2.0, 2.0);
  EXPECT_TRUE(m.RectStaysRect());
  m = skity::Matrix::Scale(0.0, 2.0);
  EXPECT_FALSE(m.RectStaysRect());
  m = skity::Matrix::RotateDeg(10.0, skity::Vec2{0.0, 0.0});
  EXPECT_FALSE(m.RectStaysRect());
  m = skity::Matrix(       //
      1.0, 3.5, 0.0, 0.0,  //
      0.0, 1.0, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,  //
      0.0, 0.0, 0.0, 1.0   //
  );
  EXPECT_FALSE(m.RectStaysRect());
  m = skity::Matrix(       //
      1.0, 0.0, 0.0, 2.8,  //
      0.0, 1.0, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,  //
      0.0, 0.0, 0.0, 1.0   //
  );
  EXPECT_FALSE(m.RectStaysRect());
  m = skity::Matrix(       //
      0.0, 2.0, 0.0, 0.0,  //
      3.0, 0.0, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,  //
      0.0, 0.0, 0.0, 1.0   //
  );
  EXPECT_TRUE(m.RectStaysRect());
}

TEST(Matrix, Rotate) {
  constexpr static float kPI = 3.1415926535897932384626;
  auto m = skity::Matrix::RotateDeg(90.0);
  EXPECT_MATRIX_EQ(m, skity::Matrix(0.0, 1.0, 0.0, 0.0,   //
                                    -1.0, 0.0, 0.0, 0.0,  //
                                    0.0, 0.0, 1.0, 0.0,   //
                                    0.0, 0.0, 0.0, 1.0));

  m = skity::Matrix::RotateDeg(90.0, skity::Vec2{100, 100});
  EXPECT_MATRIX_EQ(m, skity::Matrix::Translate(100, 100) *
                          skity::Matrix::RotateDeg(90.0) *
                          skity::Matrix::Translate(-100, -100));

  m = skity::Matrix::RotateDeg(45.0);
  EXPECT_MATRIX_EQ(m, skity::Matrix(0.707107, 0.707107, 0.0, 0.0,   //
                                    -0.707107, 0.707107, 0.0, 0.0,  //
                                    0.0, 0.0, 1.0, 0.0,             //
                                    0.0, 0.0, 0.0, 1.0));

  m = skity::Matrix::RotateRad(kPI / 4.0);
  EXPECT_MATRIX_EQ(m, skity::Matrix(0.707107, 0.707107, 0.0, 0.0,   //
                                    -0.707107, 0.707107, 0.0, 0.0,  //
                                    0.0, 0.0, 1.0, 0.0,             //
                                    0.0, 0.0, 0.0, 1.0));

  m = skity::Matrix::RotateRad(kPI / 4.0, skity::Vec2{100, 100});
  EXPECT_MATRIX_EQ(m, skity::Matrix::Translate(100, 100) *
                          skity::Matrix::RotateDeg(45.0) *
                          skity::Matrix::Translate(-100, -100));
}

TEST(Matrix, PreTranslate) {
  auto m = skity::Matrix();
  m.PreTranslate(0.87f, 0.65f);
  EXPECT_MATRIX_EQ(m, skity::Matrix(              //
                          1.0, 0.0, 0.0, 0.0,     //
                          0.0, 1.0, 0.0, 0.0,     //
                          0.0, 0.0, 1.0, 0.0,     //
                          0.87f, 0.65f, 0.0, 1.0  //
                          ));

  m.PreTranslate(0.0f, 0.0f);
  EXPECT_MATRIX_EQ(m, skity::Matrix(              //
                          1.0, 0.0, 0.0, 0.0,     //
                          0.0, 1.0, 0.0, 0.0,     //
                          0.0, 0.0, 1.0, 0.0,     //
                          0.87f, 0.65f, 0.0, 1.0  //
                          ));

  skity::Matrix s(          //
      5.3f, 0.0, 0.0, 0.0,  //
      0.0, 7.8f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      0.0, 0.0, 0.0, 1.0    //
  );
  s.PreTranslate(0.87f, 0.65f);
  EXPECT_MATRIX_EQ(s, skity::Matrix::Scale(5.3f, 7.8f) *
                          skity::Matrix::Translate(0.87f, 0.65f));
}

TEST(Matrix, PreScale) {
  auto m = skity::Matrix();
  m.PreScale(0.87f, 0.65f);
  EXPECT_MATRIX_EQ(m, skity::Matrix(             //
                          0.87f, 0.0, 0.0, 0.0,  //
                          0.0, 0.65f, 0.0, 0.0,  //
                          0.0, 0.0, 1.0, 0.0,    //
                          0.0, 0.0, 0.0, 1.0     //
                          ));

  m.PreScale(1.0f, 1.0f);
  EXPECT_MATRIX_EQ(m, skity::Matrix(             //
                          0.87f, 0.0, 0.0, 0.0,  //
                          0.0, 0.65f, 0.0, 0.0,  //
                          0.0, 0.0, 1.0, 0.0,    //
                          0.0, 0.0, 0.0, 1.0     //
                          ));

  skity::Matrix s(          //
      5.3f, 0.0, 0.0, 0.0,  //
      0.0, 7.8f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      0.0, 0.0, 0.0, 1.0    //
  );
  s.PreScale(0.87f, 0.65f);
  EXPECT_MATRIX_EQ(
      s, skity::Matrix::Scale(5.3f, 7.8f) * skity::Matrix::Scale(0.87f, 0.65f));

  skity::Matrix t(          //
      1.0, 0.0, 0.0, 0.0,   //
      0.0, 1.0, 0.0, 0.0,   //
      0.0, 0.0, 1.0, 0.0,   //
      5.3f, 7.8f, 0.0, 1.0  //
  );
  t.PreScale(0.87f, 0.65f);
  EXPECT_MATRIX_EQ(t, skity::Matrix::Translate(5.3f, 7.8f) *
                          skity::Matrix::Scale(0.87f, 0.65f));
}

TEST(Matrix, Invert) {
  auto m = skity::Matrix();
  EXPECT_TRUE(m.Invert(nullptr));

  m.Set(skity::Matrix::kMScaleX, 10.f);
  EXPECT_TRUE(m.Invert(nullptr));

  m = skity::Matrix();
  m.Set(skity::Matrix::kMScaleX, 0.f);
  EXPECT_FALSE(m.Invert(nullptr));
  m = skity::Matrix();
  m.Set(skity::Matrix::kMScaleY, 0.f);
  EXPECT_FALSE(m.Invert(nullptr));

  skity::Matrix s(          //
      5.3f, 0.0, 0.0, 0.0,  //
      0.0, 7.8f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      0.0, 0.0, 0.0, 1.0    //
  );
  EXPECT_TRUE(s.Invert(&m));
  EXPECT_MATRIX_EQ(m, skity::Matrix(                  //
                          1.f / 5.3f, 0.0, 0.0, 0.0,  //
                          0.0, 1.f / 7.8f, 0.0, 0.0,  //
                          0.0, 0.0, 1.0, 0.0,         //
                          0.0, 0.0, 0.0, 1.0          //
                          ));

  skity::Matrix t(          //
      1.0f, 0.0, 0.0, 0.0,  //
      0.0, 1.0f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      5.3f, 7.8f, 0.0, 1.0  //
  );
  EXPECT_TRUE(t.Invert(&m));
  EXPECT_MATRIX_EQ(m, skity::Matrix(              //
                          1.0f, 0.0, 0.0, 0.0,    //
                          0.0, 1.0f, 0.0, 0.0,    //
                          0.0, 0.0, 1.0, 0.0,     //
                          -5.3f, -7.8f, 0.0, 1.0  //
                          ));

  skity::Matrix st(         //
      2.0f, 0.0, 0.0, 0.0,  //
      0.0, 3.0f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      5.3f, 7.8f, 0.0, 1.0  //
  );
  EXPECT_TRUE(st.Invert(&m));
  EXPECT_MATRIX_EQ(m, glm::inverse(st));

  skity::Matrix r(          //
      2.0f, 1.0, 0.0, 6.0,  //
      0.0, 3.0f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      5.3f, 7.8f, 0.0, 1.0  //
  );
  EXPECT_TRUE(r.Invert(&m));
  EXPECT_MATRIX_EQ(m, glm::inverse(r));

  skity::Matrix st2(        //
      2.0f, 0.0, 0.0, 0.0,  //
      0.0, 3.0f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      5.3f, 7.8f, 0.0, 1.0  //
  );
  auto expected_for_st2 = glm::inverse(st2);
  EXPECT_TRUE(st2.Invert(&st2));
  EXPECT_MATRIX_EQ(st2, expected_for_st2);

  skity::Matrix r2(         //
      2.0f, 1.0, 0.0, 6.0,  //
      0.0, 3.0f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      5.3f, 7.8f, 0.0, 1.0  //
  );
  auto expected_for_r2 = glm::inverse(r2);
  EXPECT_TRUE(r2.Invert(&r2));
  EXPECT_MATRIX_EQ(r2, expected_for_r2);

  skity::Matrix m2(                         //
      1.06066012f, 1.06066012f, 0.0, 0.0,   //
      -1.06066012f, 1.06066012f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,                   //
      151.51651, -29.5495071, 0.0, 1.0      //
  );
  auto expected_for_m2 = glm::inverse(m2);
  EXPECT_TRUE(m2.Invert(&m2));
  EXPECT_MATRIX_EQ(m2, expected_for_m2);

  {
    skity::Matrix m(1.0f, 2.0f, 3.0f, 4.0f,      //
                    5.0f, 6.0f, 7.0f, 8.0f,      //
                    1.0f, 2.0f, 3.0f, 4.0f,      //
                    9.0f, 10.0f, 11.0f, 12.0f);  //
    EXPECT_FALSE(m.Invert(&m));

    skity::Matrix m2(3.0f, 1.0f, 4.0f, 2.0f,   //
                     0.0f, 0.0f, 0.0f, 0.0f,   //
                     5.0f, 6.0f, 7.0f, 8.0f,   //
                     1.0f, 1.0f, 1.0f, 1.0f);  //
    EXPECT_FALSE(m2.Invert(&m2));

    skity::Matrix m3(2.0f, 3.0f, 1.0f, 4.0f,   //
                     1.0f, 1.0f, 1.0f, 1.0f,   //
                     3.0f, 4.0f, 2.0f, 5.0f,   //
                     6.0f, 7.0f, 8.0f, 9.0f);  //
    EXPECT_FALSE(m3.Invert(&m3));
  }
}

TEST(Matrix, Set9) {
  auto m = skity::Matrix();
  float data[9] = {
      1, 2, 3,  //
      4, 5, 6,  //
      7, 8, 9   //
  };
  m.Set9(data);

  EXPECT_MATRIX_EQ(m, skity::Matrix(   //
                          1, 4, 0, 7,  //
                          2, 5, 0, 8,  //
                          0, 0, 1, 0,  //
                          3, 6, 0, 9   //
                          ));
}

TEST(Matrix, Get9) {
  auto m = skity::Matrix(  //
      1, 4, 0, 7,          //
      2, 5, 0, 8,          //
      0, 0, 1, 0,          //
      3, 6, 0, 9           //
  );
  float data[9];

  m.Get9(data);
  for (int i = 0; i < 9; i++) {
    EXPECT_EQ(data[i], i + 1);
  }
}

TEST(Matrix, MapRectWithPerspective) {
  auto m = skity::Matrix(                                 //
      0.997564077, 0.00365077169, 0.0, -0.0000696608768,  //
      0.0, 0.99862951, 0.0, 0.0000523359631,              //
      0.0, 0.0, 1.0, 0.0,                                 //
      0.243591309, -0.228030354, 0.0, 1.00173247          //
  );

  skity::Rect dst;
  m.MapRect(&dst, skity::Rect::MakeLTRB(49.9900017, 49.9900017, 150.009995,
                                        150.009995));

  EXPECT_FLOAT_EQ(dst.Left(), 49.8079414);
  EXPECT_FLOAT_EQ(dst.Top(), 49.8327866);
  EXPECT_FLOAT_EQ(dst.Right(), 150.808273);
  EXPECT_FLOAT_EQ(dst.Bottom(), 150.254211);
}

TEST(Matrix, MapPoints) {
  skity::Matrix m = skity::Matrix::Translate(100, 200);
  skity::Vec2 src[2] = {{10, 20}, {30, 40}};
  skity::Vec2 dst[2];
  skity::Point src_point[2] = {{10, 20, 0, 1}, {30, 40, 0, 1}};
  skity::Point dst_point[2];
  m.MapPoints(dst, src, 2);
  EXPECT_EQ(dst[0].x, 110);
  EXPECT_EQ(dst[0].y, 220);
  EXPECT_EQ(dst[1].x, 130);
  EXPECT_EQ(dst[1].y, 240);
  m.MapPoints(dst_point, src_point, 2);
  EXPECT_EQ(dst_point[0].x, 110);
  EXPECT_EQ(dst_point[0].y, 220);
  EXPECT_EQ(dst_point[1].x, 130);
  EXPECT_EQ(dst_point[1].y, 240);

  skity::Matrix m2 = skity::Matrix::Scale(2, 3);
  m2.MapPoints(dst, src, 2);
  EXPECT_EQ(dst[0].x, 20);
  EXPECT_EQ(dst[0].y, 60);
  EXPECT_EQ(dst[1].x, 60);
  EXPECT_EQ(dst[1].y, 120);
  m2.MapPoints(dst_point, src_point, 2);
  EXPECT_EQ(dst_point[0].x, 20);
  EXPECT_EQ(dst_point[0].y, 60);
  EXPECT_EQ(dst_point[1].x, 60);
  EXPECT_EQ(dst_point[1].y, 120);

  skity::Matrix m3 = skity::Matrix::RotateDeg(30);
  skity::Vec4 expected_dst3[2] = {
      m3 * skity::Vec4{src[0].x, src[0].y, 0.f, 1.f},
      m3 * skity::Vec4{src[1].x, src[1].y, 0.f, 1.f}};
  m3.MapPoints(dst, src, 2);
  EXPECT_EQ(dst[0].x, expected_dst3[0].x);
  EXPECT_EQ(dst[0].y, expected_dst3[0].y);
  EXPECT_EQ(dst[1].x, expected_dst3[1].x);
  EXPECT_EQ(dst[1].y, expected_dst3[1].y);
  m3.MapPoints(dst_point, src_point, 2);
  EXPECT_EQ(dst_point[0], expected_dst3[0]);
  EXPECT_EQ(dst_point[1], expected_dst3[1]);

  skity::Matrix m4 = skity::Matrix::Skew(3, 2);
  skity::Vec4 expected_dst4[2] = {
      m4 * skity::Vec4{src[0].x, src[0].y, 0.f, 1.f},
      m4 * skity::Vec4{src[1].x, src[1].y, 0.f, 1.f}};
  m4.MapPoints(dst, src, 2);
  EXPECT_EQ(dst[0].x, expected_dst4[0].x);
  EXPECT_EQ(dst[0].y, expected_dst4[0].y);
  EXPECT_EQ(dst[1].x, expected_dst4[1].x);
  EXPECT_EQ(dst[1].y, expected_dst4[1].y);
  m4.MapPoints(dst_point, src_point, 2);
  EXPECT_EQ(dst_point[0], expected_dst4[0]);
  EXPECT_EQ(dst_point[1], expected_dst4[1]);
}

TEST(Matrix, MapPointsWithPerspective) {
  auto m = skity::Matrix(                                 //
      0.997564077, 0.00365077169, 0.0, -0.0000696608768,  //
      0.0, 0.99862951, 0.0, 0.0000523359631,              //
      0.0, 0.0, 1.0, 0.0,                                 //
      0.243591309, -0.228030354, 0.0, 1.00173247          //
  );

  skity::Vec2 src[2] = {{49.9900017, 49.9900017}, {150.009995, 150.009995}};
  skity::Vec2 dst[2];
  skity::Point src_point[2] = {{49.9900017, 49.9900017, 0, 1},
                               {150.009995, 150.009995, 0, 1}};
  skity::Point dst_point[2];
  m.MapPoints(dst, src, 2);
  m.MapPoints(dst_point, src_point, 2);
  skity::Vec4 expected_dst[2] = {m * src_point[0], m * src_point[1]};
  ASSERT_TRUE(expected_dst[0].w != 1);
  ASSERT_TRUE(expected_dst[1].w != 1);
  EXPECT_FLOAT_EQ(dst[0].x, expected_dst[0].x * (1.f / expected_dst[0].w));
  EXPECT_FLOAT_EQ(dst[0].y, expected_dst[0].y * (1.f / expected_dst[0].w));
  EXPECT_FLOAT_EQ(dst[1].x, expected_dst[1].x * (1.f / expected_dst[1].w));
  EXPECT_FLOAT_EQ(dst[1].y, expected_dst[1].y * (1.f / expected_dst[1].w));
  EXPECT_EQ(dst_point[0], expected_dst[0]);
  EXPECT_EQ(dst_point[1], expected_dst[1]);
}
