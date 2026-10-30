// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <limits>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rrect.hpp>
#include <skity/geometry/stroke.hpp>
#include <skity/graphic/path_builder.hpp>
#include <type_traits>
#include <utility>

namespace {
using skity::Path;
using skity::PathBuilder;
using skity::Rect;
using skity::Vec2;

static_assert(!std::is_copy_constructible_v<PathBuilder>);
static_assert(!std::is_copy_assignable_v<PathBuilder>);
static_assert(std::is_nothrow_move_constructible_v<PathBuilder>);
static_assert(std::is_nothrow_move_assignable_v<PathBuilder>);

void ExpectSamePath(const Path& actual, const Path& expected) {
  EXPECT_EQ(actual, expected);
  EXPECT_EQ(actual.GetSegmentMasks(), expected.GetSegmentMasks());
  EXPECT_EQ(actual.GetIsAType(), expected.GetIsAType());
  EXPECT_EQ(actual.GetBounds(), expected.GetBounds());
  EXPECT_EQ(actual.IsFinite(), expected.IsFinite());
  EXPECT_EQ(actual.GetConvexityType(), expected.GetConvexityType());
  if (actual.IsConvex() && expected.IsConvex()) {
    EXPECT_EQ(actual.GetFirstDirection(), expected.GetFirstDirection());
  }
  skity::RRect a, e;
  EXPECT_EQ(actual.IsSimpleRRect(&a), expected.IsSimpleRRect(&e));
  EXPECT_EQ(a, e);
}

template <typename Target>
void BuildCommands(Target& p) {
  p.LineTo(10, 10);
  p.MoveTo(1, 2).MoveTo(3, 4).LineTo(7, 8);
  p.QuadTo(10, 2, 12, 3).ConicTo(14, 4, 16, 5, 0.75f);
  p.CubicTo(18, 6, 20, 7, 22, 8).Close().Close();
  p.LineTo(24, 9);
  p.SetLastPt(25, 10);
  p.ConicTo(26, 10, 28, 12, 0);
  p.ConicTo(30, 11, 32, 10, 1);
  p.ConicTo(34, 9, 36, 8, std::numeric_limits<float>::infinity());
  p.ConicTo(38, 7, 40, 6, std::numeric_limits<float>::quiet_NaN());
  p.SetFillType(Path::PathFillType::kEvenOdd);
}

TEST(PathBuilder, CommandsMatchLegacyPath) {
  Path expected;
  PathBuilder builder;
  BuildCommands(expected);
  BuildCommands(builder);
  EXPECT_EQ(builder.CountPoints(), expected.CountPoints());
  EXPECT_EQ(builder.CountVerbs(), expected.CountVerbs());
  EXPECT_EQ(builder.GetPoint(0), expected.GetPoint2D(0));
  EXPECT_EQ(builder.GetPoint(-1), Vec2(0, 0));
  EXPECT_EQ(builder.GetPoint(10000), Vec2(0, 0));
  EXPECT_EQ(builder.GetLastMovePt(), expected.GetLastMovePt2D());
  EXPECT_EQ(builder.GetBounds(), expected.GetBounds());
  ExpectSamePath(builder.Detach(), expected);
}

TEST(PathBuilder, Vec2CommandsMatchScalars) {
  PathBuilder b;
  b.MoveTo(Vec2{1, 2}).LineTo(Vec2{3, 4});
  b.QuadTo(Vec2{5, 6}, Vec2{7, 8});
  b.ConicTo(Vec2{9, 10}, Vec2{11, 12}, 0.5f);
  b.CubicTo(Vec2{13, 14}, Vec2{15, 16}, Vec2{17, 18});
  b.SetLastPt(Vec2{19, 20});
  Path expected;
  expected.MoveTo(1, 2).LineTo(3, 4).QuadTo(5, 6, 7, 8);
  expected.ConicTo(9, 10, 11, 12, 0.5f).CubicTo(13, 14, 15, 16, 17, 18);
  expected.SetLastPt(19, 20);
  ExpectSamePath(b.Detach(), expected);
}

TEST(PathBuilder, ShapesAndArcsMatchLegacyPath) {
  Rect rect{1, 2, 101, 82};
  float radii[] = {4, 5, 6, 7, 8, 9, 10, 11};
  skity::RRect rrect;
  rrect.SetRectXY(rect, 12, 13);
  auto check = [](auto command) {
    Path expected;
    PathBuilder builder;
    command(expected);
    command(builder);
    ExpectSamePath(builder.Detach(), expected);
  };
  for (auto dir : {Path::Direction::kCW, Path::Direction::kCCW}) {
    for (uint32_t start = 0; start < 24; ++start) {
      check([&](auto& p) { p.AddRect(rect, dir, start); });
      check([&](auto& p) { p.AddOval(rect, dir, start); });
      check([&](auto& p) { p.AddRRect(rrect, dir, start); });
    }
    check([&](auto& p) { p.AddRect(1, 2, 101, 82, dir); });
    check([&](auto& p) { p.AddOval(rect, dir); });
    check([&](auto& p) { p.AddCircle(50, 40, 20, dir); });
    check([&](auto& p) { p.AddRoundRect(rect, 12, 13, dir); });
    check([&](auto& p) { p.AddRoundRect(rect, radii, dir); });
    check([&](auto& p) { p.AddRRect(rrect, dir); });
    check([&](auto& p) { p.MoveTo(2, 3).ArcTo(20, 3, 20, 25, 5); });
    for (auto arc : {Path::kSmall, Path::kLarge}) {
      check(
          [&](auto& p) { p.MoveTo(1, 2).ArcTo(20, 15, 30, arc, dir, 40, 30); });
    }
  }
  for (float sweep : {-720.f, -90.f, 0.f, 90.f, 360.f, 720.f}) {
    for (bool force_move : {false, true}) {
      check([&](auto& p) { p.ArcTo(rect, 15, sweep, force_move); });
    }
  }
}

TEST(PathBuilder, SnapshotIsIndependent) {
  PathBuilder builder;
  builder.AddRect({0, 0, 10, 20});
  Path first = builder.Snapshot();
  Path second = builder.Snapshot();
  ASSERT_NE(first.Points(), second.Points());
  builder.LineTo(30, 40);
  second.LineTo(50, 60);
  EXPECT_EQ(first.CountVerbs(), 5u);
  EXPECT_EQ(builder.GetPoint(static_cast<int>(builder.CountPoints() - 1)),
            Vec2(30, 40));
  EXPECT_EQ(second.GetPoint2D(static_cast<int>(second.CountPoints() - 1)),
            Vec2(50, 60));
}

TEST(PathBuilder, DetachTransfersAllArraysAndResetsSource) {
  Path original;
  BuildCommands(original);
  Path expected = original;
  const auto* points = original.Points();
  const auto* verbs = original.VerbsBegin();
  const auto* weights = original.ConicWeights();
  PathBuilder builder(std::move(original));
  EXPECT_TRUE(original.IsEmpty());
  EXPECT_EQ(original.GetFillType(), Path::PathFillType::kWinding);
  Path result = builder.Detach();
  EXPECT_EQ(result.Points(), points);
  EXPECT_EQ(result.VerbsBegin(), verbs);
  EXPECT_EQ(result.ConicWeights(), weights);
  ExpectSamePath(result, expected);
  EXPECT_TRUE(builder.IsEmpty());
  EXPECT_EQ(builder.GetFillType(), Path::PathFillType::kWinding);
  EXPECT_EQ(builder.GetLastMovePt(), Vec2(0, 0));
  builder.LineTo(8, 9);
  Path line;
  line.LineTo(8, 9);
  ExpectSamePath(builder.Detach(), line);
  original.LineTo(3, 4);
  EXPECT_EQ(original.GetPoint2D(0), Vec2(0, 0));
}

TEST(PathBuilder, DetachToRecyclesDestinationArrays) {
  PathBuilder capacity;
  capacity.Reserve(128, 128, 32);
  capacity.MoveTo(0, 0).ConicTo(1, 2, 3, 4, 0.5f);
  Path destination = capacity.Detach();
  const auto* points = destination.Points();
  const auto* verbs = destination.VerbsBegin();
  const auto* weights = destination.ConicWeights();
  PathBuilder builder;
  builder.AddRoundRect({0, 0, 100, 100}, 10, 20, Path::Direction::kCCW);
  builder.SetFillType(Path::PathFillType::kEvenOdd);
  Path expected = builder.Snapshot();
  builder.DetachTo(&destination);
  ExpectSamePath(destination, expected);
  EXPECT_EQ(builder.GetFillType(), Path::PathFillType::kWinding);
  EXPECT_TRUE(builder.IsEmpty());
  builder.Reserve(2, 2, 1);
  for (int i = 0; i < 12; ++i) builder.ConicTo(i, 1, i + 1, 2, 0.5f);
  builder.Reset();
  builder.ConicTo(4, 5, 6, 7, 0.5f);
  Path reused = builder.Detach();
  EXPECT_EQ(reused.Points(), points);
  EXPECT_EQ(reused.VerbsBegin(), verbs);
  EXPECT_EQ(reused.ConicWeights(), weights);
  builder.LineTo(8, 9);
  builder.DetachTo(nullptr);
  EXPECT_FALSE(builder.IsEmpty());
}

TEST(PathBuilder, MoveAndSwapPreserveValues) {
  PathBuilder a;
  a.AddOval({0, 0, 20, 40}, Path::Direction::kCCW);
  a.SetFillType(Path::PathFillType::kEvenOdd);
  Path expected = a.Snapshot();
  PathBuilder b(std::move(a));
  EXPECT_TRUE(a.IsEmpty());
  a.LineTo(1, 2);
  a = std::move(b);
  EXPECT_TRUE(b.IsEmpty());
  EXPECT_EQ(b.GetFillType(), Path::PathFillType::kWinding);
  auto* same = &a;
  a = std::move(*same);
  ExpectSamePath(a.Snapshot(), expected);
  b.LineTo(5, 6);
  Path line = b.Snapshot();
  a.Swap(b);
  ExpectSamePath(a.Detach(), line);
  ExpectSamePath(b.Detach(), expected);
}

TEST(PathBuilder, ImportedConvexityIsInvalidated) {
  Path source;
  source.AddRect({0, 0, 10, 10});
  ASSERT_TRUE(source.IsConvex());
  PathBuilder copied(source);
  PathBuilder moved(std::move(source));
  for (auto* builder : {&copied, &moved}) {
    builder->AddRect({20, 20, 30, 30});
    EXPECT_FALSE(builder->Detach().IsConvex());
  }
}

TEST(PathBuilder, ImportAndAppendPreserveLegacyComponents) {
  Path original;
  BuildCommands(original);
  skity::Matrix transform;
  transform.Set(2, 3, 3);
  transform.Set(3, 3, 2);
  Path source = original.CopyWithMatrix(transform);
  PathBuilder imported(source);
  ExpectSamePath(imported.Detach(), source);
  for (auto mode : {Path::AddMode::kAppend, Path::AddMode::kExtend}) {
    PathBuilder builder;
    Path expected;
    builder.MoveTo(4, 5);
    expected.MoveTo(4, 5);
    builder.AddPath(source, mode)
        .AddPath(source, 7, 8, mode)
        .AddPath(source, transform, mode);
    expected.AddPath(source, mode)
        .AddPath(source, 7, 8, mode)
        .AddPath(source, transform, mode);
    ExpectSamePath(builder.Detach(), expected);
  }
}

TEST(PathBuilder, ReverseAppendAndSelfAppend) {
  Path source;
  BuildCommands(source);
  PathBuilder builder;
  Path expected;
  builder.ReverseAddPath(source);
  expected.ReverseAddPath(source);
  builder.ReversePathTo(source);
  expected.ReversePathTo(source);
  PathBuilder other(source);
  builder.ReversePathTo(other);
  expected.ReversePathTo(source);
  ExpectSamePath(builder.Detach(), expected);
  builder.Reserve(0, 0, 0);
  for (int i = 0; i < 64; ++i) builder.LineTo(i, i * 2);
  Path snapshot = builder.Snapshot();
  expected = snapshot;
  expected.ReversePathTo(snapshot);
  builder.ReversePathTo(builder);
  ExpectSamePath(builder.Detach(), expected);
  Path empty;
  builder.ReverseAddPath(empty).ReversePathTo(empty).ReversePathTo(builder);
  EXPECT_TRUE(builder.IsEmpty());
}

TEST(Path, SwapCompleteValue) {
  Path a, b;
  a.AddRoundRect({0, 0, 100, 80}, 12, 14, Path::Direction::kCCW);
  a.SetFillType(Path::PathFillType::kEvenOdd);
  b.AddOval({20, 30, 50, 70}, Path::Direction::kCW);
  ASSERT_TRUE(a.IsConvex());
  ASSERT_TRUE(b.IsConvex());
  Path old_a = a, old_b = b;
  a.Swap(b);
  ExpectSamePath(a, old_b);
  ExpectSamePath(b, old_a);
  a.LineTo(120, 130);
  old_b.LineTo(120, 130);
  EXPECT_EQ(a, old_b);
  b.Swap(b);
  ExpectSamePath(b, old_a);
  Path non_finite;
  non_finite.LineTo(1, 2).MoveTo(std::numeric_limits<float>::infinity(), 1);
  ASSERT_FALSE(non_finite.IsFinite());
  b.Swap(non_finite);
  EXPECT_FALSE(b.IsFinite());
  EXPECT_TRUE(non_finite.IsFinite());
  ExpectSamePath(non_finite, old_a);
}

TEST(Path, StrokePreservesDestinationFillType) {
  Path source;
  source.MoveTo(1, 2).LineTo(30, 40).QuadTo(50, 5, 60, 40).Close();
  skity::Paint paint;
  paint.SetStrokeWidth(4);
  skity::Stroke stroke(paint);
  Path winding, even_odd;
  even_odd.SetFillType(Path::PathFillType::kEvenOdd);
  stroke.StrokePath(source, &winding);
  stroke.StrokePath(source, &even_odd);
  ASSERT_FALSE(winding.IsEmpty());
  EXPECT_EQ(even_odd.GetFillType(), Path::PathFillType::kEvenOdd);
  EXPECT_EQ(winding.GetFillType(), Path::PathFillType::kWinding);
  even_odd.SetFillType(Path::PathFillType::kWinding);
  ExpectSamePath(winding, even_odd);
}
}  // namespace
