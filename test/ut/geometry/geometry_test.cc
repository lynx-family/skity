// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/geometry/geometry.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "src/geometry/math.hpp"

TEST(QUAD, tangents) {
  std::vector<std::array<skity::Point, 3>> pts = {
      {skity::Point{10, 20, 0, 1}, skity::Point{10, 20, 0, 1},
       skity::Point{20, 30, 0, 1}},
      {skity::Point{10, 20, 0, 1}, skity::Point{15, 25, 0, 1},
       skity::Point{20, 30, 0, 1}},
      {skity::Point{10, 20, 0, 1}, skity::Point{20, 30, 0, 1},
       skity::Point{20, 30, 0, 1}},
  };

  size_t count = pts.size();
  for (size_t i = 0; i < count; i++) {
    skity::Vector start = skity::QuadCoeff::EvalQuadTangentAt(pts[i], 0);
    skity::Vector mid = skity::QuadCoeff::EvalQuadTangentAt(pts[i], .5f);
    skity::Vector end = skity::QuadCoeff::EvalQuadTangentAt(pts[i], 1.f);

    EXPECT_TRUE(start.x && start.y);
    EXPECT_TRUE(mid.x && mid.y);
    EXPECT_TRUE(end.x && end.y);
    EXPECT_TRUE(skity::FloatNearlyZero(skity::CrossProduct(start, mid)));
    EXPECT_TRUE(skity::FloatNearlyZero(skity::CrossProduct(mid, end)));
  }
}

static inline bool Vec2NearlyEqual(skity::Vec2 value, skity::Vec2 expect) {
  return skity::FloatNearlyZero(value.x - expect.x) &&
         skity::FloatNearlyZero(value.y - expect.y);
}

TEST(Geometry, CircleInterpolation) {
  {
    skity::Vec2 start_unit_vec = {1, 0};
    skity::Vec2 end_unit_vec = {0, 1};
    std::vector<skity::Vec2> result =
        skity::CircleInterpolation(start_unit_vec, end_unit_vec, 2);
    EXPECT_TRUE(Vec2NearlyEqual(result[0], {::sqrtf(2) / 2, ::sqrtf(2) / 2}));
    EXPECT_TRUE(Vec2NearlyEqual(result[1], {0, 1}));
  }

  {
    skity::Vec2 start_unit_vec = {1, 0};
    skity::Vec2 end_unit_vec = {0, 1};
    std::vector<skity::Vec2> result =
        skity::CircleInterpolation(start_unit_vec, end_unit_vec, 3);
    EXPECT_TRUE(Vec2NearlyEqual(result[0], {::sqrtf(3) / 2, 0.5}));
    EXPECT_TRUE(Vec2NearlyEqual(result[1], {0.5, ::sqrtf(3) / 2}));
    EXPECT_TRUE(Vec2NearlyEqual(result[2], {0, 1}));

    std::swap(start_unit_vec, end_unit_vec);
    result = skity::CircleInterpolation(start_unit_vec, end_unit_vec, 3);
    EXPECT_TRUE(Vec2NearlyEqual(result[0], {0.5, ::sqrtf(3) / 2}));
    EXPECT_TRUE(Vec2NearlyEqual(result[1], {::sqrtf(3) / 2, 0.5}));
    EXPECT_TRUE(Vec2NearlyEqual(result[2], {1, 0}));
  }

  {
    skity::Vec2 start_unit_vec = {1, 0};
    skity::Vec2 end_unit_vec = {-1, 0};
    std::vector<skity::Vec2> result =
        skity::CircleInterpolation(start_unit_vec, end_unit_vec, 4);
    EXPECT_TRUE(Vec2NearlyEqual(result[0], {::sqrtf(2) / 2, ::sqrtf(2) / 2}));
    EXPECT_TRUE(Vec2NearlyEqual(result[1], {0, 1}));
    EXPECT_TRUE(Vec2NearlyEqual(result[2], {-::sqrtf(2) / 2, ::sqrtf(2) / 2}));
    EXPECT_TRUE(Vec2NearlyEqual(result[3], {-1, 0}));

    std::swap(start_unit_vec, end_unit_vec);
    result = skity::CircleInterpolation(start_unit_vec, end_unit_vec, 4);
    EXPECT_TRUE(Vec2NearlyEqual(result[0], {-::sqrtf(2) / 2, -::sqrtf(2) / 2}));
    EXPECT_TRUE(Vec2NearlyEqual(result[1], {0, -1}));
    EXPECT_TRUE(Vec2NearlyEqual(result[2], {::sqrtf(2) / 2, -::sqrtf(2) / 2}));
    EXPECT_TRUE(Vec2NearlyEqual(result[3], {1, 0}));
  }

  {
    skity::Vec2 start_unit_vec = {1, 0};
    float x = 0.996;
    skity::Vec2 end_unit_vec = {x, ::sqrtf(1 - x * x)};
    std::vector<skity::Vec2> result =
        skity::CircleInterpolation(start_unit_vec, end_unit_vec, 4);
    const auto cos01 = skity::CrossProduct(start_unit_vec, result[0]);
    const auto cos12 = skity::CrossProduct(result[0], result[1]);
    const auto cos23 = skity::CrossProduct(result[1], result[2]);
    const auto cos34 = skity::CrossProduct(result[2], result[3]);
    EXPECT_TRUE(skity::FloatNearlyZero(cos01 - cos12));
    EXPECT_TRUE(skity::FloatNearlyZero(cos01 - cos23));
    EXPECT_TRUE(skity::FloatNearlyZero(cos01 - cos34));
    EXPECT_TRUE(Vec2NearlyEqual(result[3], {x, ::sqrtf(1 - x * x)}));

    std::swap(start_unit_vec, end_unit_vec);
    result = skity::CircleInterpolation(start_unit_vec, end_unit_vec, 4);
    const auto cos01_prime = skity::CrossProduct(start_unit_vec, result[0]);
    const auto cos12_prime = skity::CrossProduct(result[0], result[1]);
    const auto cos23_prime = skity::CrossProduct(result[1], result[2]);
    const auto cos34_prime = skity::CrossProduct(result[2], result[3]);
    EXPECT_TRUE(skity::FloatNearlyZero(cos01_prime - cos12_prime));
    EXPECT_TRUE(skity::FloatNearlyZero(cos01_prime - cos23_prime));
    EXPECT_TRUE(skity::FloatNearlyZero(cos01_prime - cos34_prime));
    EXPECT_TRUE(Vec2NearlyEqual(result[3], {1, 0}));
  }

  {
    skity::Vec2 start_unit_vec = {1, 0};
    float x = -0.996;
    skity::Vec2 end_unit_vec = {x, ::sqrtf(1 - x * x)};
    std::vector<skity::Vec2> result =
        skity::CircleInterpolation(start_unit_vec, end_unit_vec, 4);
    const auto cos01 = skity::CrossProduct(start_unit_vec, result[0]);
    const auto cos12 = skity::CrossProduct(result[0], result[1]);
    const auto cos23 = skity::CrossProduct(result[1], result[2]);
    const auto cos34 = skity::CrossProduct(result[2], result[3]);
    EXPECT_TRUE(skity::FloatNearlyZero(cos01 - cos12));
    EXPECT_TRUE(skity::FloatNearlyZero(cos01 - cos23));
    EXPECT_TRUE(skity::FloatNearlyZero(cos01 - cos34));
    EXPECT_TRUE(Vec2NearlyEqual(result[3], {x, ::sqrtf(1 - x * x)}));

    std::swap(start_unit_vec, end_unit_vec);
    result = skity::CircleInterpolation(start_unit_vec, end_unit_vec, 4);
    const auto cos01_prime = skity::CrossProduct(start_unit_vec, result[0]);
    const auto cos12_prime = skity::CrossProduct(result[0], result[1]);
    const auto cos23_prime = skity::CrossProduct(result[1], result[2]);
    const auto cos34_prime = skity::CrossProduct(result[2], result[3]);
    EXPECT_TRUE(skity::FloatNearlyZero(cos01_prime - cos12_prime));
    EXPECT_TRUE(skity::FloatNearlyZero(cos01_prime - cos23_prime));
    EXPECT_TRUE(skity::FloatNearlyZero(cos01_prime - cos34_prime));
    EXPECT_TRUE(Vec2NearlyEqual(result[3], {1, 0}));
  }
}
