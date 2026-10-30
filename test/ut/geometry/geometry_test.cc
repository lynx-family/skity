// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/geometry/geometry.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "src/geometry/cubic.hpp"
#include "src/geometry/math.hpp"
#include "src/geometry/wangs_formula.hpp"
#include "src/graphic/path_visitor.hpp"

TEST(Cubic2D, PreservesQuadraticAndPointCurves) {
  skity::Cubic quadratic({0, 0}, {2, 4}, {4, 4}, {6, 0});
  auto quads = quadratic.ToQuads();
  ASSERT_EQ(quads.size(), 1u);
  EXPECT_EQ(quads[0][0], (skity::Vec2{0, 0}));
  EXPECT_EQ(quads[0][1], (skity::Vec2{3, 6}));
  EXPECT_EQ(quads[0][2], (skity::Vec2{6, 0}));

  skity::Cubic point({7, -3}, {7, -3}, {7, -3}, {7, -3});
  quads = point.ToQuads();
  ASSERT_EQ(quads.size(), 1u);
  for (const auto& p : quads[0]) {
    EXPECT_EQ(p, (skity::Vec2{7, -3}));
  }
}

TEST(Cubic2D, SubdividedCurveKeepsContinuityAndApproximationAccuracy) {
  skity::Cubic cubic({0, 0}, {0, 100}, {100, -100}, {100, 0});
  auto quads = cubic.ToQuads();
  ASSERT_GT(quads.size(), 1u);
  EXPECT_EQ(quads.front()[0], (skity::Vec2{0, 0}));
  EXPECT_EQ(quads.back()[2], (skity::Vec2{100, 0}));
  for (size_t i = 0; i < quads.size(); ++i) {
    const auto& quad = quads[i];
    if (i > 0) {
      EXPECT_EQ(quad[0], quads[i - 1][2]);
    }
    for (float u : {0.f, 0.25f, 0.5f, 0.75f, 1.f}) {
      double t = (i + static_cast<double>(u)) / quads.size();
      skity::Vec2 expected{100 * t * t * (3 - 2 * t),
                           300 * t * (1 - t) * (1 - 2 * t)};
      auto actual = (1 - u) * (1 - u) * quad[0] + 2 * (1 - u) * u * quad[1] +
                    u * u * quad[2];
      EXPECT_LE((actual - expected).Length(), 0.1001f);
    }
  }
}

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

TEST(CurveSegmentLimit, SegmentLimits) {
  using skity::ClampCurveSegments;
  for (float value : {0.f, 0.5f, 32.25f, 255.5f, 256.f}) {
    EXPECT_FLOAT_EQ(ClampCurveSegments(value), value);
  }
  for (float value : {256.5f, 3.e9f, std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::infinity()}) {
    EXPECT_FLOAT_EQ(ClampCurveSegments(value), 256.f);
    EXPECT_EQ(static_cast<int>(std::ceil(ClampCurveSegments(value))) + 1, 257);
  }
  for (float value : {-1.f, -std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
    EXPECT_FLOAT_EQ(ClampCurveSegments(value), 0.f);
  }
  EXPECT_FLOAT_EQ(ClampCurveSegments(2.f * ClampCurveSegments(200.f)), 256.f);
}

TEST(CurveSegmentLimit, RawCurveEstimates) {
  using namespace skity::wangs_formula;
  const skity::Vec2 normal[] = {{0, 0}, {20, 40}, {40, 0}, {60, 20}};
  EXPECT_FLOAT_EQ(Quadratic(4.f, normal), Root4(QuadraticP4(4.f, normal)));
  EXPECT_FLOAT_EQ(Cubic(4.f, normal), Root4(CubicP4(4.f, normal)));
  EXPECT_FLOAT_EQ(Conic(4.f, normal, 0.7f),
                  std::sqrt(ConicP2(4.f, normal, 0.7f)));
  for (float scale : {1.e8f, 1.e20f}) {
    const skity::Vec2 large[] = {
        {0, 0}, {0, scale}, {scale, 0}, {scale, scale}};
    EXPECT_GT(Quadratic(4.f, large), 256.f);
    EXPECT_GT(Cubic(4.f, large), 256.f);
    EXPECT_GT(Conic(4.f, large, 0.7f), 256.f);
    EXPECT_GT(QuadraticP4(4.f, large), 256.f * 256.f * 256.f * 256.f);
  }
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const skity::Vec2 invalid[] = {
      {nan, nan}, {nan, nan}, {nan, nan}, {nan, nan}};
  EXPECT_TRUE(std::isnan(Quadratic(4.f, invalid)));
  EXPECT_TRUE(std::isnan(Cubic(4.f, invalid)));
  EXPECT_TRUE(std::isnan(Conic(4.f, invalid, 0.7f)));
  const skity::Vec2 zero[4] = {};
  EXPECT_FLOAT_EQ(Quadratic(4.f, zero), 0.f);
  EXPECT_FLOAT_EQ(Cubic(4.f, zero), 0.f);
  EXPECT_FLOAT_EQ(Conic(4.f, zero, 1.f), 0.f);
}

namespace {
class CountingPathVisitor : public skity::PathVisitor {
 public:
  CountingPathVisitor() : PathVisitor(true, skity::Matrix{}) {}
  int segments = 0;
  skity::Vec2 end = {};

 protected:
  void OnBeginPath() override { segments = 0; }
  void OnEndPath() override {}
  void OnMoveTo(const skity::Vec2&) override {}
  void OnLineTo(const skity::Vec2&, const skity::Vec2& p) override {
    ++segments;
    end = p;
  }
  void OnQuadTo(const skity::Vec2&, const skity::Vec2&,
                const skity::Vec2&) override {}
  void OnConicTo(const skity::Vec2&, const skity::Vec2&, const skity::Vec2&,
                 float) override {}
  void OnCubicTo(const skity::Vec2&, const skity::Vec2&, const skity::Vec2&,
                 const skity::Vec2&) override {}
  void OnClose() override {}
};
}  // namespace

TEST(CurveSegmentLimit, PathVisitorBoundsLargeCurves) {
  CountingPathVisitor visitor;
  skity::Path quad;
  quad.MoveTo(0, 0);
  quad.QuadTo(0, 1.e20f, 10, 0);
  visitor.VisitPath(quad, false);
  EXPECT_EQ(visitor.segments, 256);
  EXPECT_EQ(visitor.end, (skity::Vec2{10, 0}));

  skity::Path cubic;
  cubic.MoveTo(0, 0);
  cubic.CubicTo(0, 1.e20f, 1.e20f, 0, 10, 0);
  visitor.VisitPath(cubic, false);
  EXPECT_EQ(visitor.segments, 256);
  EXPECT_EQ(visitor.end, (skity::Vec2{10, 0}));
}
