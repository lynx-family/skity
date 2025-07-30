// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
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

  m.SetScaleX(10.f);
  EXPECT_TRUE(m.Invert(nullptr));

  m = skity::Matrix();
  m.SetScaleX(0.f);
  EXPECT_FALSE(m.Invert(nullptr));
  m = skity::Matrix();
  m.SetScaleY(0.f);
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
  EXPECT_MATRIX_EQ(m, glm::inverse(reinterpret_cast<glm::mat4&>(st)));

  skity::Matrix r(          //
      2.0f, 1.0, 0.0, 6.0,  //
      0.0, 3.0f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      5.3f, 7.8f, 0.0, 1.0  //
  );
  EXPECT_TRUE(r.Invert(&m));
  EXPECT_MATRIX_EQ(m, glm::inverse(reinterpret_cast<glm::mat4&>(r)));

  skity::Matrix st2(        //
      2.0f, 0.0, 0.0, 0.0,  //
      0.0, 3.0f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      5.3f, 7.8f, 0.0, 1.0  //
  );
  auto expected_for_st2 = glm::inverse(reinterpret_cast<glm::mat4&>(st2));
  EXPECT_TRUE(st2.Invert(&st2));
  EXPECT_MATRIX_EQ(st2, expected_for_st2);

  skity::Matrix r2(         //
      2.0f, 1.0, 0.0, 6.0,  //
      0.0, 3.0f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,   //
      5.3f, 7.8f, 0.0, 1.0  //
  );
  auto expected_for_r2 = glm::inverse(reinterpret_cast<glm::mat4&>(r2));
  EXPECT_TRUE(r2.Invert(&r2));
  EXPECT_MATRIX_EQ(r2, expected_for_r2);

  skity::Matrix m2(                         //
      1.06066012f, 1.06066012f, 0.0, 0.0,   //
      -1.06066012f, 1.06066012f, 0.0, 0.0,  //
      0.0, 0.0, 1.0, 0.0,                   //
      151.51651, -29.5495071, 0.0, 1.0      //
  );
  auto expected_for_m2 = glm::inverse(reinterpret_cast<glm::mat4&>(m2));
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

TEST(Matrix, Basic) {
  skity::Matrix m;
  EXPECT_MATRIX_EQ(m, skity::Matrix(   //
                          1, 0, 0, 0,  //
                          0, 1, 0, 0,  //
                          0, 0, 1, 0,  //
                          0, 0, 0, 1   //
                          ));
  skity::Matrix m2{1};
  EXPECT_MATRIX_EQ(m2, skity::Matrix(   //
                           1, 0, 0, 0,  //
                           0, 1, 0, 0,  //
                           0, 0, 1, 0,  //
                           0, 0, 0, 1   //
                           ));

  skity::Matrix m3 = skity::Matrix::Translate(50, 80);
  EXPECT_MATRIX_EQ(m3, skity::Matrix(    //
                           1, 0, 0, 0,   //
                           0, 1, 0, 0,   //
                           0, 0, 1, 0,   //
                           50, 80, 0, 1  //
                           ));

  skity::Matrix m4{3};
  EXPECT_MATRIX_EQ(m4, skity::Matrix(   //
                           3, 0, 0, 0,  //
                           0, 3, 0, 0,  //
                           0, 0, 3, 0,  //
                           0, 0, 0, 3   //
                           ));
  skity::Matrix m5 = skity::Matrix::Scale(3, 2);
  EXPECT_MATRIX_EQ(m5, skity::Matrix(   //
                           3, 0, 0, 0,  //
                           0, 2, 0, 0,  //
                           0, 0, 1, 0,  //
                           0, 0, 0, 1   //
                           ));
  skity::Matrix m6 = skity::Matrix::Skew(3, 2);
  EXPECT_MATRIX_EQ(m6, skity::Matrix(   //
                           1, 2, 0, 0,  //
                           3, 1, 0, 0,  //
                           0, 0, 1, 0,  //
                           0, 0, 0, 1   //
                           ));

  skity::Matrix m7 = skity::Matrix{
      1, 2, 3,  //
      4, 5, 6,  //
      7, 8, 9   //
  };

  EXPECT_MATRIX_EQ(m7, skity::Matrix(   //
                           1, 4, 0, 7,  //
                           2, 5, 0, 8,  //
                           0, 0, 1, 0,  //
                           3, 6, 0, 9   //
                           ));
  skity::Matrix m8 = m7;
  EXPECT_MATRIX_EQ(m8, skity::Matrix(   //
                           1, 4, 0, 7,  //
                           2, 5, 0, 8,  //
                           0, 0, 1, 0,  //
                           3, 6, 0, 9   //
                           ));
  m8 = m6;
  EXPECT_MATRIX_EQ(m8, skity::Matrix(   //
                           1, 2, 0, 0,  //
                           3, 1, 0, 0,  //
                           0, 0, 1, 0,  //
                           0, 0, 0, 1   //
                           ));

  EXPECT_TRUE(m8 == m6);
  EXPECT_TRUE(m8 != m7);

  EXPECT_EQ(m8.IsIdentity(), false);
  m8.Reset();
  EXPECT_EQ(m8.IsIdentity(), true);
  EXPECT_MATRIX_EQ(m8, skity::Matrix());
  EXPECT_EQ(m8.IsFinite(), true);

  float inf = std::numeric_limits<float>::infinity();
  skity::Matrix m9 = skity::Matrix(  //
      inf, 2, 0, 0,                  //
      3, 1, 0, 0,                    //
      0, 0, 1, 0,                    //
      0, 0, 0, 1                     //
  );

  float nan = std::numeric_limits<float>::quiet_NaN();
  skity::Matrix m10 = skity::Matrix(  //
      nan, 2, 0, 0,                   //
      3, 1, 0, 0,                     //
      0, 0, 1, 0,                     //
      0, 0, 0, 1                      //
  );
  EXPECT_EQ(m10.IsFinite(), false);
}

TEST(Matrix, IsSimilarity) {
  skity::Matrix m1 = skity::Matrix::Translate(50, 80);
  EXPECT_EQ(m1.IsSimilarity(), true);

  skity::Matrix m2 = skity::Matrix::Scale(3, 3);
  EXPECT_EQ(m1.IsSimilarity(), true);

  skity::Matrix m3 = skity::Matrix::Scale(3, 4);
  EXPECT_EQ(m3.IsSimilarity(), false);

  skity::Matrix m4 = skity::Matrix::Skew(3, 2);
  EXPECT_EQ(m4.IsSimilarity(), false);

  skity::Matrix m5 = skity::Matrix::RotateDeg(30);
  EXPECT_EQ(m5.IsSimilarity(), true);

  skity::Matrix m6 = skity::Matrix{1, 0, 0, 1,  //
                                   0, 1, 0, 0,  //
                                   0, 0, 1, 0,  //
                                   0, 0, 0, 1};
  EXPECT_EQ(m6.IsSimilarity(), false);
}

TEST(Matrix, SetAndGet) {
  skity::Matrix m1{
      1,  2,  3,  4,   //
      5,  6,  7,  8,   //
      9,  10, 11, 12,  //
      13, 14, 15, 16,  //
  };
  EXPECT_EQ(m1.GetScaleX(), 1);
  EXPECT_EQ(m1.GetScaleY(), 6);
  EXPECT_EQ(m1.GetSkewX(), 5);
  EXPECT_EQ(m1.GetSkewY(), 2);
  EXPECT_EQ(m1.GetTranslateX(), 13);
  EXPECT_EQ(m1.GetTranslateY(), 14);
  EXPECT_EQ(m1.GetPersp0(), 4);
  EXPECT_EQ(m1.GetPersp1(), 8);
  EXPECT_EQ(m1.GetPersp2(), 16);

  for (size_t i = 0; i < 4; i++) {
    for (size_t j = 0; j < 4; j++) {
      EXPECT_EQ(m1.Get(i, j), m1[j][i]);
    }
  }

  skity::Matrix m2;
  m2.SetScaleX(1);
  m2.SetScaleY(6);
  m2.SetSkewX(5);
  m2.SetSkewY(2);
  m2.SetTranslateX(13);
  m2.SetTranslateY(14);
  m2.SetPersp0(4);
  m2.SetPersp1(8);
  m2.SetPersp2(16);
  EXPECT_MATRIX_EQ(m2, skity::Matrix{
                           1, 2, 0, 4,     //
                           5, 6, 0, 8,     //
                           0, 0, 1, 0,     //
                           13, 14, 0, 16,  //
                       });

  for (size_t i = 0; i < 4; i++) {
    for (size_t j = 0; j < 4; j++) {
      m2.Set(i, j, i * 40 + j * 10 + 10);
    }
  }

  EXPECT_MATRIX_EQ(m2, skity::Matrix{
                           10, 50, 90, 130,   //
                           20, 60, 100, 140,  //
                           30, 70, 110, 150,  //
                           40, 80, 120, 160,  //
                       });
}

TEST(Matrix, Determinant) {
  skity::Matrix m{
      1,  2,  3,  4,   //
      5,  6,  7,  8,   //
      9,  10, 11, 12,  //
      13, 14, 15, 16,  //
  };

  EXPECT_FLOAT_EQ(m.Determinant(),
                  glm::determinant(reinterpret_cast<glm::mat4&>(m)));

  skity::Matrix m2 = skity::Matrix::Translate(100, 200);
  EXPECT_FLOAT_EQ(m2.Determinant(),
                  glm::determinant(reinterpret_cast<glm::mat4&>(m2)));

  skity::Matrix m3 = skity::Matrix::Scale(100, 200);
  EXPECT_FLOAT_EQ(m3.Determinant(),
                  glm::determinant(reinterpret_cast<glm::mat4&>(m3)));

  skity::Matrix m4 = skity::Matrix::RotateDeg(45);
  EXPECT_FLOAT_EQ(m4.Determinant(),
                  glm::determinant(reinterpret_cast<glm::mat4&>(m4)));

  skity::Matrix m5 = skity::Matrix::Skew(3, 2);
  EXPECT_FLOAT_EQ(m5.Determinant(),
                  glm::determinant(reinterpret_cast<glm::mat4&>(m5)));
}

TEST(Matrix, Transpose) {
  skity::Matrix m{
      1,  2,  3,  4,   //
      5,  6,  7,  8,   //
      9,  10, 11, 12,  //
      13, 14, 15, 16,  //
  };

  m.Transpose();
  EXPECT_MATRIX_EQ(m, skity::Matrix{
                          1, 5, 9, 13,   //
                          2, 6, 10, 14,  //
                          3, 7, 11, 15,  //
                          4, 8, 12, 16,  //
                      });
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

TEST(Matrix, PreConcatAndPostConcat) {
  skity::Matrix src = skity::Matrix{
      1,  2,  3,  4,   //
      5,  6,  7,  8,   //
      9,  10, 11, 12,  //
      13, 14, 15, 16,  //
  };
  auto m = src;
  skity::Matrix m1 = skity::Matrix::Scale(2, 3);
  skity::Matrix m2 = skity::Matrix::RotateDeg(30);
  skity::Matrix m3 = skity::Matrix::Translate(10, 20);
  skity::Matrix m4 = skity::Matrix::Skew(3, 2);
  skity::Matrix m5 = skity::Matrix::RotateDeg(60, {10, 20});

  m.PreScale(2, 3);
  m.PreRotate(30);
  m.PreTranslate(10, 20);
  m.PreConcat(m4);
  m.PreRotate(60, 10, 20);
  EXPECT_MATRIX_EQ(m, src * m1 * m2 * m3 * m4 * m5);

  m = src;
  m.PostScale(2, 3);
  m.PostRotate(30);
  m.PostTranslate(10, 20);
  m.PostSkew(3, 2);
  m.PostConcat(m4);
  m.PostRotate(60, 10, 20);
  EXPECT_MATRIX_EQ(m, m5 * m4 * m4 * m3 * m2 * m1 * src);

  m = src;
  m.PreScale(3, 5, 60, 100);
  EXPECT_MATRIX_EQ(m, src * skity::Matrix::Translate(60, 100) *
                          skity::Matrix::Scale(3, 5) *
                          skity::Matrix::Translate(-60, -100));
}

TEST(Matrix, OnlyScaleAndTranslate) {
  skity::Matrix m = skity::Matrix::Scale(3, 5);
  EXPECT_TRUE(m.OnlyScale());
  EXPECT_FALSE(m.OnlyTranslate());
  EXPECT_TRUE(m.OnlyScaleAndTranslate());

  m = skity::Matrix::Translate(3, 5);
  EXPECT_FALSE(m.OnlyScale());
  EXPECT_TRUE(m.OnlyTranslate());
  EXPECT_TRUE(m.OnlyScaleAndTranslate());

  m = skity::Matrix{};
  EXPECT_TRUE(m.OnlyScale());
  EXPECT_TRUE(m.OnlyTranslate());
  EXPECT_TRUE(m.OnlyScaleAndTranslate());

  m = skity::Matrix::RotateDeg(30);
  EXPECT_FALSE(m.OnlyScale());
  EXPECT_FALSE(m.OnlyTranslate());
  EXPECT_FALSE(m.OnlyScaleAndTranslate());

  m = skity::Matrix::Skew(3, 2);
  EXPECT_FALSE(m.OnlyScale());
  EXPECT_FALSE(m.OnlyTranslate());
  EXPECT_FALSE(m.OnlyScaleAndTranslate());

  m = skity::Matrix::Scale(3, 2) * skity::Matrix::Translate(50, 100);
  EXPECT_FALSE(m.OnlyScale());
  EXPECT_FALSE(m.OnlyTranslate());
  EXPECT_TRUE(m.OnlyScaleAndTranslate());

  m.SetPersp0(2);
  EXPECT_FALSE(m.OnlyScale());
  EXPECT_FALSE(m.OnlyTranslate());
  EXPECT_FALSE(m.OnlyScaleAndTranslate());
}

TEST(Matrix, HasPersp) {
  skity::Matrix m;
  EXPECT_FALSE(m.HasPersp());

  m = skity::Matrix::Translate(3, 5);
  EXPECT_FALSE(m.HasPersp());

  m = skity::Matrix::Scale(3, 2);
  EXPECT_FALSE(m.HasPersp());

  m = skity::Matrix::RotateDeg(30);
  EXPECT_FALSE(m.HasPersp());

  m = skity::Matrix::Skew(3, 2);
  EXPECT_FALSE(m.HasPersp());

  m.SetPersp0(2);
  EXPECT_TRUE(m.HasPersp());

  m.Reset();
  m.SetPersp1(3);
  EXPECT_TRUE(m.HasPersp());

  m.Reset();
  m.SetPersp2(4);
  EXPECT_TRUE(m.HasPersp());
}

TEST(Matrix, MapRect) {
  skity::Matrix m = skity::Matrix::Scale(3, 2);
  skity::Rect r = {10, 20, 30, 40};
  skity::Rect expected = {30, 40, 90, 80};
  EXPECT_EQ(m.MapRect(r), expected);

  m = skity::Matrix::Translate(100, 200);
  r = {10, 20, 30, 40};
  expected = {110, 220, 130, 240};
  EXPECT_EQ(m.MapRect(r), expected);

  m = skity::Matrix::RotateDeg(45);
  r = {-10, -10, 10, 10};
  expected = {-10 * std::sqrt(2.f), -10 * std::sqrt(2.f), 10 * std::sqrt(2.f),
              10 * std::sqrt(2.f)};
  EXPECT_EQ(m.MapRect(r), expected);

  m = skity::Matrix::Skew(1, 0);
  r = {0, 0, 100, 100};
  expected = {0, 0, 200, 100};
  EXPECT_EQ(m.MapRect(r), expected);
}

TEST(Matrix, Access) {
  skity::Matrix m{
      1,  2,  3,  4,   //
      5,  6,  7,  8,   //
      9,  10, 11, 12,  //
      13, 14, 15, 16,  //
  };

  skity::Point expected[4] = {
      {1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};

  EXPECT_EQ(m[0], expected[0]);
  EXPECT_EQ(m[1], expected[1]);
  EXPECT_EQ(m[2], expected[2]);
  EXPECT_EQ(m[3], expected[3]);

  m[0] = {-1, -2, -3, -4};
  m[1] = {-5, -6, -7, -8};
  m[2] = {-9, -10, -11, -12};
  m[3] = {-13, -14, -15, -16};

  EXPECT_EQ(m[0], -expected[0]);
  EXPECT_EQ(m[1], -expected[1]);
  EXPECT_EQ(m[2], -expected[2]);
  EXPECT_EQ(m[3], -expected[3]);

  skity::Matrix m2 = skity::Matrix::Translate(100, 200);
  EXPECT_EQ(m2[3][0], 100);
  EXPECT_EQ(m2[3][1], 200);

  skity::Matrix m3 = skity::Matrix::Scale(3, 2);
  EXPECT_EQ(m3[0][0], 3);
  EXPECT_EQ(m3[1][1], 2);

  skity::Matrix m4 = skity::Matrix::Skew(3, 2);
  EXPECT_EQ(m4[1][0], 3);
  EXPECT_EQ(m4[0][1], 2);

  m4[3][3] = 5;
  EXPECT_EQ(m4[3][3], 5);
}
