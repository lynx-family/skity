// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <skity/graphic/path.hpp>
#include <skity/graphic/path_measure.hpp>
#include <type_traits>

namespace skity {
namespace {

// Keep the legacy signatures available during the downstream roll.
static_assert(std::is_same_v<Point, Vec4>);
static_assert(
    std::is_same_v<decltype(&Path::GetLastPt), bool (Path::*)(Point*) const>);
static_assert(
    std::is_same_v<decltype(&Path::Points), const Point* (Path::*)() const>);
using LegacyIterNext = Path::Verb (Path::Iter::*)(Point*);
using LegacyRawIterNext = Path::Verb (Path::RawIter::*)(Point*);
static_assert(static_cast<LegacyIterNext>(&Path::Iter::Next) != nullptr);
static_assert(static_cast<LegacyRawIterNext>(&Path::RawIter::Next) != nullptr);
static_assert(std::is_same_v<decltype(&PathMeasure::GetPosTan),
                             bool (PathMeasure::*)(float, Point*, Vector*)>);

constexpr Vec2 kSentinel{1234.f, -5678.f};

Path MixedPath() {
  Path path;
  path.MoveTo(1, 2)
      .LineTo(3, 4)
      .QuadTo(5, 6, 7, 8)
      .ConicTo(9, 10, 11, 12, 0.5f)
      .CubicTo(13, 14, 15, 16, 17, 18)
      .Close();
  path.MoveTo(-3, -4).LineTo(-5, -6);
  return path;
}

TEST(Path2D, PointQueriesHandleEmptyContoursAndInvalidIndices) {
  Path path;
  Vec2 last = kSentinel;
  EXPECT_FALSE(path.GetLastPt2D(&last));
  EXPECT_EQ(last, Vec2{});
  EXPECT_FALSE(path.GetLastPt2D(nullptr));
  EXPECT_EQ(path.GetLastMovePt2D(), Vec2{});
  EXPECT_EQ(path.GetPoint2D(0), Vec2{});
  EXPECT_EQ(path.GetPoint2D(-1), Vec2{});

  path.MoveTo(1, 2).LineTo(3, 4).MoveTo(5, 6).QuadTo(7, 8, 9, 10).Close();
  EXPECT_TRUE(path.GetLastPt2D(&last));
  EXPECT_TRUE(path.GetLastPt2D(nullptr));
  EXPECT_EQ(last, (Vec2{9, 10}));
  EXPECT_EQ(path.GetPoint2D(2), (Vec2{5, 6}));
  EXPECT_EQ(path.GetLastMovePt2D(), (Vec2{5, 6}));
  EXPECT_EQ(path.GetPoint2D(-1), Vec2{});
  EXPECT_EQ(path.GetPoint2D(static_cast<int>(path.CountPoints())), Vec2{});
  EXPECT_EQ(path.GetPoint2D(std::numeric_limits<int>::max()), Vec2{});

  path.LineTo(11, 12);
  EXPECT_EQ(path.GetLastMovePt2D(), (Vec2{5, 6}));
  path.Reset();
  EXPECT_EQ(path.GetLastMovePt2D(), Vec2{});
}

TEST(Path2D, QueriesCopyXYWithoutChangingLegacyGeometryOrDividingByW) {
  Path source;
  source.MoveTo(2, 4).LineTo(6, 8);
  Matrix matrix;
  matrix.Set(2, 3, 7.f);
  matrix.Set(3, 3, 2.f);
  Path path = source.CopyWithMatrix(matrix);
  const Point* storage = path.Points();
  ASSERT_EQ(path.GetPoint(0), (Point{2, 4, 7, 2}));
  EXPECT_EQ(path.GetPoint2D(0), (Vec2{2, 4}));
  EXPECT_EQ(path.GetLastMovePt2D(), (Vec2{2, 4}));
  Vec2 last;
  ASSERT_TRUE(path.GetLastPt2D(&last));
  EXPECT_EQ(last, (Vec2{6, 8}));
  Vec2 points[2];
  EXPECT_EQ(path.CopyPoints2D(points, 2), 2u);
  EXPECT_EQ(points[0], (Vec2{2, 4}));
  EXPECT_EQ(points[1], (Vec2{6, 8}));
  EXPECT_EQ(path.Points(), storage);
  EXPECT_EQ(storage[1], (Point{6, 8, 7, 2}));

  Path::RawIter iter(path);
  Vec2 segment[4];
  ASSERT_EQ(iter.Next(segment), Path::Verb::kMove);
  EXPECT_EQ(segment[0], (Vec2{2, 4}));
}

TEST(Path2D, CopyPointsRespectsCapacityAndOwnsItsOutput) {
  Path path;
  path.MoveTo(1, 2).LineTo(3, 4).ConicTo(5, 6, 7, 8, 0.5f);
  std::array<Vec2, 6> output;
  output.fill(kSentinel);
  EXPECT_EQ(path.CopyPoints2D(nullptr, 0), 4u);
  EXPECT_EQ(path.CopyPoints2D(output.data(), 0), 0u);
  EXPECT_EQ(output[0], kSentinel);
  EXPECT_EQ(path.CopyPoints2D(output.data(), 2), 2u);
  EXPECT_EQ(output[0], (Vec2{1, 2}));
  EXPECT_EQ(output[1], (Vec2{3, 4}));
  EXPECT_EQ(output[2], kSentinel);
  EXPECT_EQ(path.CopyPoints2D(output.data(), output.size()), 4u);
  EXPECT_EQ(output[2], (Vec2{5, 6}));
  EXPECT_EQ(output[3], (Vec2{7, 8}));
  EXPECT_EQ(output[4], kSentinel);
  EXPECT_EQ(output[5], kSentinel);
  path.Reset();
  EXPECT_EQ(path.CopyPoints2D(output.data(), output.size()), 0u);
  EXPECT_EQ(output[0], (Vec2{1, 2}));
  EXPECT_EQ(output[3], (Vec2{7, 8}));
}

template <typename Iterator>
void CheckIteration2D(Iterator legacy, Iterator planar) {
  for (size_t step = 0; step < 32; ++step) {
    Point expected[4];
    std::array<Vec2, 4> actual;
    actual.fill(kSentinel);
    auto verb = legacy.Next(expected);
    ASSERT_EQ(planar.Next(actual.data()), verb);
    size_t count = 0;
    switch (verb) {
      case Path::Verb::kMove:
        count = 1;
        break;
      case Path::Verb::kLine:
        count = 2;
        break;
      case Path::Verb::kQuad:
      case Path::Verb::kConic:
        count = 3;
        break;
      case Path::Verb::kCubic:
        count = 4;
        break;
      case Path::Verb::kClose:
      case Path::Verb::kDone:
        break;
    }
    for (size_t i = 0; i < count; ++i) {
      EXPECT_EQ(actual[i], Vec2{expected[i]});
    }
    for (size_t i = count; i < actual.size(); ++i) {
      EXPECT_EQ(actual[i], kSentinel);
    }
    if (verb == Path::Verb::kConic) {
      EXPECT_EQ(planar.ConicWeight(), legacy.ConicWeight());
      EXPECT_EQ(planar.ConicWeight(), 0.5f);
    }
    if (verb == Path::Verb::kDone) {
      return;
    }
  }
  FAIL() << "Iterator did not reach Done";
}

TEST(Path2D, RawIterationPreservesSegmentsAndConicWeights) {
  Path path = MixedPath();
  CheckIteration2D(Path::RawIter{path}, Path::RawIter{path});
  Path::RawIter iter;
  Vec2 points[4];
  EXPECT_EQ(iter.Next(points), Path::Verb::kDone);
  iter.SetPath(path);
  EXPECT_EQ(iter.Peek(), Path::Verb::kMove);
  EXPECT_EQ(iter.Next(points), Path::Verb::kMove);
  EXPECT_EQ(iter.Peek(), Path::Verb::kLine);
}

TEST(Path2D, IterationPreservesExplicitAndForcedClosing) {
  Path path = MixedPath();
  for (bool force_close : {false, true}) {
    CheckIteration2D(Path::Iter{path, force_close},
                     Path::Iter{path, force_close});
  }
  Path line;
  line.MoveTo(1, 2).LineTo(3, 4);
  Path::Iter iter(line, true);
  Vec2 points[4];
  EXPECT_TRUE(iter.IsClosedContour());
  ASSERT_EQ(iter.Next(points), Path::Verb::kMove);
  ASSERT_EQ(iter.Next(points), Path::Verb::kLine);
  ASSERT_EQ(iter.Next(points), Path::Verb::kLine);
  EXPECT_TRUE(iter.IsCloseLine());
  EXPECT_EQ(points[0], (Vec2{3, 4}));
  EXPECT_EQ(points[1], (Vec2{1, 2}));
  EXPECT_EQ(iter.Next(points), Path::Verb::kClose);
  EXPECT_EQ(iter.Next(points), Path::Verb::kDone);
}

TEST(Path2D, IteratorsCanMixLegacyAndPlanarReads) {
  Path path;
  path.MoveTo(1, 2).LineTo(3, 4).QuadTo(5, 6, 7, 8);
  Path::RawIter iter(path);
  Point legacy[4];
  Vec2 planar[4];
  ASSERT_EQ(iter.Next(legacy), Path::Verb::kMove);
  ASSERT_EQ(iter.Next(planar), Path::Verb::kLine);
  EXPECT_EQ(planar[0], (Vec2{1, 2}));
  EXPECT_EQ(planar[1], (Vec2{3, 4}));
  ASSERT_EQ(iter.Next(legacy), Path::Verb::kQuad);
  EXPECT_EQ(legacy[2], (Point{7, 8, 0, 1}));
}

TEST(Path2D, LineQueryCopiesEndpointsOnlyForASingleLine) {
  Path path;
  Vec2 line[2] = {kSentinel, kSentinel};
  EXPECT_FALSE(path.IsLine2D(line));
  EXPECT_EQ(line[0], kSentinel);
  EXPECT_EQ(line[1], kSentinel);
  path.MoveTo(1, 2).LineTo(3, 4);
  EXPECT_TRUE(path.IsLine2D(nullptr));
  ASSERT_TRUE(path.IsLine2D(line));
  EXPECT_EQ(line[0], (Vec2{1, 2}));
  EXPECT_EQ(line[1], (Vec2{3, 4}));
  path.Close();
  EXPECT_FALSE(path.IsLine2D(line));
  EXPECT_EQ(line[0], (Vec2{1, 2}));
  EXPECT_EQ(line[1], (Vec2{3, 4}));
}

TEST(PathMeasure2D, MatchesLegacyCurvePositionsTangentsAndDistanceClamping) {
  Path path = MixedPath();
  PathMeasure measure(path, false);
  do {
    for (float fraction : {-1.f, 0.f, 0.2f, 0.5f, 0.8f, 1.f, 2.f}) {
      float distance = measure.GetLength() * fraction;
      Point expected_position;
      Vector expected_tangent;
      Vec2 position;
      Vec2 tangent;
      ASSERT_TRUE(
          measure.GetPosTan(distance, &expected_position, &expected_tangent));
      ASSERT_TRUE(measure.GetPosTan2D(distance, &position, &tangent));
      EXPECT_EQ(position, Vec2{expected_position});
      EXPECT_EQ(tangent, Vec2{expected_tangent});
      EXPECT_TRUE(measure.GetPosTan2D(distance, nullptr, &tangent));
      EXPECT_EQ(tangent, Vec2{expected_tangent});
      EXPECT_TRUE(measure.GetPosTan2D(distance, &position, nullptr));
      EXPECT_EQ(position, Vec2{expected_position});
      EXPECT_TRUE(measure.GetPosTan2D(distance, nullptr, nullptr));
      EXPECT_TRUE(measure.GetPosTan(distance, nullptr, nullptr));
    }
  } while (measure.NextContour());
}

TEST(PathMeasure2D, FailureLeavesOutputsUnchanged) {
  PathMeasure measure;
  Vec2 position = kSentinel;
  Vec2 tangent = kSentinel;
  EXPECT_FALSE(measure.GetPosTan2D(0, &position, &tangent));
  EXPECT_EQ(position, kSentinel);
  EXPECT_EQ(tangent, kSentinel);
  Path path;
  path.MoveTo(1, 2).LineTo(1, 2);
  measure.SetPath(&path, false);
  EXPECT_FALSE(measure.GetPosTan2D(0, &position, &tangent));
  path.LineTo(3, 4);
  measure.SetPath(&path, false);
  EXPECT_FALSE(measure.GetPosTan2D(std::numeric_limits<float>::quiet_NaN(),
                                   &position, &tangent));
  EXPECT_EQ(position, kSentinel);
  EXPECT_EQ(tangent, kSentinel);
}

TEST(PathMeasure2D, LegacyMeasureKeepsCanonicalPositionAndTangentComponents) {
  Path source = MixedPath();
  Matrix matrix;
  matrix.Set(2, 3, 7.f);
  matrix.Set(3, 3, 2.f);
  Path transformed = source.CopyWithMatrix(matrix);
  PathMeasure expected(source, false);
  PathMeasure measure(transformed, false);
  do {
    EXPECT_FLOAT_EQ(measure.GetLength(), expected.GetLength());
    for (float t : {0.f, 0.25f, 0.5f, 0.75f, 1.f}) {
      Point position, expected_position;
      Vector tangent, expected_tangent;
      float distance = t * expected.GetLength();
      ASSERT_TRUE(
          expected.GetPosTan(distance, &expected_position, &expected_tangent));
      ASSERT_TRUE(measure.GetPosTan(distance, &position, &tangent));
      EXPECT_EQ(position, expected_position);
      EXPECT_EQ(tangent, expected_tangent);
      EXPECT_EQ(position.z, 0.f);
      EXPECT_EQ(position.w, 1.f);
      EXPECT_EQ(tangent.z, 0.f);
      EXPECT_EQ(tangent.w, 0.f);
    }
    if (!expected.NextContour()) {
      EXPECT_FALSE(measure.NextContour());
      break;
    }
    ASSERT_TRUE(measure.NextContour());
  } while (true);
}

}  // namespace
}  // namespace skity
