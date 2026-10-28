// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <atomic>
#include <limits>
#include <skity/graphic/path.hpp>
#include <thread>
#include <utility>
#include <vector>

namespace skity {
namespace {

Path MixedPath() {
  Path path;
  path.MoveTo(1, 2)
      .LineTo(3, 4)
      .QuadTo(5, 6, 7, 8)
      .ConicTo(9, 10, 11, 12, 0.5f)
      .CubicTo(13, 14, 15, 16, 17, 18)
      .Close();
  return path;
}

TEST(PathCow, CopiesShareAllArraysAndMetadataEditsStayLocal) {
  Path source = MixedPath();
  Path copy(source);
  Path assigned;
  assigned = source;
  EXPECT_EQ(copy.Points(), source.Points());
  EXPECT_EQ(assigned.VerbsBegin(), source.VerbsBegin());
  EXPECT_EQ(copy.ConicWeights(), source.ConicWeights());
  copy.SetFillType(Path::PathFillType::kEvenOdd);
  copy.SetFirstDirection(Path::Direction::kCW);
  copy.SetConvexityType(Path::ConvexityType::kConcave);
  EXPECT_EQ(copy.Points(), source.Points());
  EXPECT_EQ(source.GetFillType(), Path::PathFillType::kWinding);
  EXPECT_NE(source.GetFirstDirection(), copy.GetFirstDirection());
  EXPECT_NE(copy, source);
  assigned = assigned;
  EXPECT_EQ(assigned, source);
}

TEST(PathCow, EveryGeometryWriterPreservesTheSharedSource) {
  using Edit = void (*)(Path&);
  const Edit edits[] = {
      [](Path& p) { p.MoveTo(90, 91); },
      [](Path& p) { p.LineTo(90, 91); },
      [](Path& p) { p.QuadTo(80, 81, 90, 91); },
      [](Path& p) { p.ConicTo(80, 81, 90, 91, 0.7f); },
      [](Path& p) { p.ConicTo(80, 81, 90, 91, 0.f); },
      [](Path& p) { p.ConicTo(80, 81, 90, 91, 1.f); },
      [](Path& p) {
        p.ConicTo(80, 81, 90, 91, std::numeric_limits<float>::infinity());
      },
      [](Path& p) { p.CubicTo(70, 71, 80, 81, 90, 91); },
      [](Path& p) { p.SetLastPt(90, 91); },
      [](Path& p) { p.AddRect(30, 30, 90, 90); },
      [](Path& p) { p.AddOval(Rect::MakeLTRB(30, 30, 90, 90)); },
      [](Path& p) { p.AddRoundRect(Rect::MakeWH(90, 90), 5, 5); },
      [](Path& p) { p.ArcTo(Rect::MakeWH(90, 90), 0, 180, false); },
      [](Path& p) { p.AddPath(MixedPath()); },
      [](Path& p) { p.AddPath(MixedPath(), Path::AddMode::kExtend); },
      [](Path& p) { p.ReverseAddPath(MixedPath()); },
      [](Path& p) { p.ReversePathTo(MixedPath()); },
  };
  for (size_t i = 0; i < std::size(edits); ++i) {
    SCOPED_TRACE(i);
    Path source = MixedPath();
    const auto bounds = source.GetBounds();
    const auto convexity = source.GetConvexityType();
    Path copy(source);
    edits[i](copy);
    EXPECT_NE(copy.Points(), source.Points());
    EXPECT_EQ(source, MixedPath());
    EXPECT_EQ(source.GetBounds(), bounds);
    EXPECT_EQ(source.GetConvexityType(), convexity);
  }
}

TEST(PathCow, ReplacingMoveAndClosingDetachBeforeWriting) {
  Path source;
  source.MoveTo(1, 2);
  Path replaced(source);
  replaced.MoveTo(3, 4);
  EXPECT_EQ(source.GetPoint(0), (Point{1, 2, 0, 1}));
  EXPECT_EQ(replaced.CountPoints(), 1u);
  Path closed(source);
  closed.Close();
  EXPECT_EQ(source.CountVerbs(), 1u);
  EXPECT_EQ(closed.CountVerbs(), 2u);
  const auto* verbs = closed.VerbsBegin();
  Path snapshot(closed);
  closed.Close();
  EXPECT_EQ(closed.VerbsBegin(), verbs);
  closed.LineTo(5, 6);
  ASSERT_EQ(closed.CountPoints(), 3u);
  EXPECT_EQ(closed.GetPoint(1), source.GetPoint(0));
  EXPECT_EQ(snapshot.CountPoints(), 1u);
}

TEST(PathCow, ResetReusesUniqueCapacityButDropsSharedStorage) {
  Path path = MixedPath();
  const auto* points = path.Points();
  const auto* verbs = path.VerbsBegin();
  const auto* weights = path.ConicWeights();
  path.Reset().MoveTo(1, 2).ConicTo(3, 4, 5, 6, 0.5f);
  EXPECT_EQ(path.Points(), points);
  EXPECT_EQ(path.VerbsBegin(), verbs);
  EXPECT_EQ(path.ConicWeights(), weights);
  Path snapshot(path);
  path.Reset();
  EXPECT_TRUE(path.IsEmpty());
  EXPECT_TRUE(path.IsFinite());
  EXPECT_TRUE(path.GetBounds().IsEmpty());
  path.MoveTo(9, 10);
  EXPECT_NE(path.Points(), snapshot.Points());
  EXPECT_EQ(snapshot.GetPoint(0), (Point{1, 2, 0, 1}));
  snapshot.Reset();
  EXPECT_EQ(snapshot.Points(), points);
}

TEST(PathCow, LastOwnerCanEditWithoutCloning) {
  Path path = MixedPath();
  const auto* points = path.Points();
  {
    Path copy(path);
  }
  path.SetLastPt(20, 21);
  EXPECT_EQ(path.Points(), points);
}

TEST(PathCow, MovesTransferStorageAndLeaveReusableEmptyValues) {
  Path source = MixedPath();
  const auto* points = source.Points();
  Path moved(std::move(source));
  EXPECT_EQ(moved.Points(), points);
  EXPECT_TRUE(source.IsEmpty());
  source.LineTo(1, 2);
  EXPECT_EQ(source.CountPoints(), 2u);
  Path assigned = MixedPath();
  assigned = std::move(moved);
  EXPECT_EQ(assigned.Points(), points);
  EXPECT_TRUE(moved.IsEmpty());
  moved.AddRect(1, 2, 3, 4);
  EXPECT_EQ(assigned, MixedPath());
  auto* self = &assigned;
  assigned = std::move(*self);
  EXPECT_EQ(assigned.Points(), points);
}

TEST(PathCow, SwapIncludesFillDirectionAndShapeMetadata) {
  Path rect;
  rect.AddRect(0, 0, 20, 30, Path::Direction::kCW);
  rect.SetFillType(Path::PathFillType::kEvenOdd);
  Path rounded;
  rounded.AddRoundRect(Rect::MakeWH(40, 50), 5, 6, Path::Direction::kCCW);
  const Path old_rect(rect), old_rounded(rounded);
  rect.Swap(rounded);
  EXPECT_EQ(rect, old_rounded);
  EXPECT_EQ(rounded, old_rect);
  EXPECT_EQ(rect.GetFirstDirection(), Path::Direction::kCCW);
  EXPECT_EQ(rounded.GetFirstDirection(), Path::Direction::kCW);
  RRect rr;
  EXPECT_TRUE(rect.IsSimpleRRect(&rr));
  EXPECT_EQ(rr.GetSimpleRadii(), (Vec2{5, 6}));
  EXPECT_EQ(rounded.GetIsAType(), Path::IsAType::kRect);
}

TEST(PathCow, SelfAndSharedAppendMatchIndependentSource) {
  for (auto mode : {Path::AddMode::kAppend, Path::AddMode::kExtend}) {
    Path source = MixedPath();
    Path expected = MixedPath();
    expected.AddPath(source, Matrix::Translate(30, 40), mode);
    Path self = MixedPath();
    self.AddPath(self, Matrix::Translate(30, 40), mode);
    EXPECT_EQ(self, expected);
    Path shared(source);
    shared.AddPath(source, Matrix::Translate(30, 40), mode);
    EXPECT_EQ(shared, expected);
    EXPECT_EQ(source, MixedPath());
  }
}

TEST(PathCow, SelfAndSharedReverseKeepSourcePointersAlive) {
  for (bool whole_path : {false, true}) {
    Path source = MixedPath();
    Path expected = MixedPath();
    Path self = MixedPath();
    Path shared(source);
    if (whole_path) {
      expected.ReverseAddPath(source);
      self.ReverseAddPath(self);
      shared.ReverseAddPath(source);
    } else {
      expected.ReversePathTo(source);
      self.ReversePathTo(self);
      shared.ReversePathTo(source);
    }
    EXPECT_EQ(self, expected);
    EXPECT_EQ(shared, expected);
    EXPECT_EQ(source, MixedPath());
  }
}

TEST(PathCow, ClosedAppendUsesPointOffsetForSubsequentImplicitMove) {
  Path destination;
  destination.MoveTo(0, 0).CubicTo(1, 2, 3, 4, 5, 6);
  Path source;
  source.MoveTo(10, 20).LineTo(30, 40).Close();
  destination.AddPath(source).LineTo(50, 60);
  ASSERT_EQ(destination.CountPoints(), 8u);
  EXPECT_EQ(destination.GetPoint(6), (Point{10, 20, 0, 1}));
}

TEST(PathCow, MutationsInvalidateBoundsFiniteConvexityAndDirection) {
  Path source;
  source.AddRect(0, 0, 10, 10);
  ASSERT_TRUE(source.IsConvex());
  Path copy(source);
  copy.MoveTo(20, 20).LineTo(30, 30).Close();
  EXPECT_FALSE(copy.IsConvex());
  EXPECT_EQ(copy.GetBounds(), Rect::MakeLTRB(0, 0, 30, 30));
  EXPECT_TRUE(source.IsConvex());
  EXPECT_EQ(source.GetBounds(), Rect::MakeWH(10, 10));
  copy.MoveTo(std::numeric_limits<float>::infinity(), 0);
  EXPECT_FALSE(copy.IsFinite());
  EXPECT_TRUE(source.IsFinite());
  copy.SetLastPt(40, 50);
  EXPECT_TRUE(copy.IsFinite());
  source.SetLastPt(7, 8);
  EXPECT_EQ(source.GetFirstDirection(), Path::Direction::kUnknown);
  EXPECT_EQ(source.GetIsAType(), Path::IsAType::kGeneral);
}

TEST(PathCow, ClosingTrailingMoveInvalidatesBounds) {
  Path path;
  path.MoveTo(0, 0).LineTo(10, 10).MoveTo(30, 40);
  EXPECT_EQ(path.GetBounds(), Rect::MakeWH(10, 10));
  Path snapshot(path);
  path.Close();
  EXPECT_EQ(path.GetBounds(), Rect::MakeWH(30, 40));
  EXPECT_EQ(snapshot.GetBounds(), Rect::MakeWH(10, 10));
}

TEST(PathCow, TransformsOwnGeometryAndRecomputeDirection) {
  Path source;
  source.AddRect(0, 0, 10, 20);
  source.SetFillType(Path::PathFillType::kEvenOdd);
  ASSERT_TRUE(source.IsConvex());
  auto reflected = source.CopyWithMatrix(Matrix::Scale(-1, 1));
  EXPECT_NE(reflected.Points(), source.Points());
  EXPECT_EQ(reflected.GetBounds(), Rect::MakeLTRB(-10, 0, 0, 20));
  EXPECT_TRUE(reflected.IsConvex());
  EXPECT_NE(reflected.GetFirstDirection(), source.GetFirstDirection());
  auto scaled = source.CopyWithScale(2);
  EXPECT_EQ(scaled.GetBounds(), Rect::MakeWH(20, 40));
  EXPECT_EQ(scaled.GetFillType(), source.GetFillType());
  scaled.SetLastPt(100, 200);
  EXPECT_EQ(source.GetBounds(), Rect::MakeWH(10, 20));
}

TEST(PathCow, EmptyValuesStayIndependentAcrossThreads) {
  const Path empty;
  std::vector<std::thread> workers;
  for (int t = 0; t < 8; ++t) {
    workers.emplace_back([&] {
      for (int i = 0; i < 100; ++i) {
        Path path(empty);
        path.Close().Reset().LineTo(3, 4);
        ASSERT_EQ(path.CountPoints(), 2u);
        EXPECT_EQ(path.GetPoint(0), (Point{0, 0, 0, 1}));
        EXPECT_TRUE(empty.IsEmpty());
        EXPECT_TRUE(empty.GetBounds().IsEmpty());
      }
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  EXPECT_TRUE(empty.IsEmpty());
}

TEST(PathCow, ConcurrentColdQueriesAndIndependentEdits) {
  Path source = MixedPath();
  const auto expected_bounds = MixedPath().GetBounds();
  std::vector<std::thread> workers;
  for (int t = 0; t < 8; ++t) {
    workers.emplace_back([&, copy = source]() mutable {
      for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(source.GetBounds(), expected_bounds);
        EXPECT_TRUE(source.IsFinite());
        EXPECT_EQ(source.GetConvexityType(), copy.GetConvexityType());
        EXPECT_EQ(copy.GetBounds(), expected_bounds);
      }
      copy.SetLastPt(80, 90);
      EXPECT_EQ(copy.GetPoint(copy.CountPoints() - 1), (Point{80, 90, 0, 1}));
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  EXPECT_EQ(source, MixedPath());
}

TEST(PathCow, LastReaderReleaseSynchronizesEditAndReset) {
  for (bool reset : {false, true}) {
    for (int i = 0; i < 100; ++i) {
      Path path;
      path.MoveTo(1, 2).LineTo(3, 4);
      std::atomic<bool> released{false};
      float observed = 0;
      std::thread reader([copy = path, &released, &observed]() mutable {
        observed = copy.Points()[1].x;
        copy.Reset();
        // Schedule the write without supplying the storage's happens-before.
        released.store(true, std::memory_order_relaxed);
      });
      while (!released.load(std::memory_order_relaxed)) {
        std::this_thread::yield();
      }
      if (reset) {
        path.Reset().MoveTo(5, 6);
      } else {
        path.SetLastPt(5, 6);
      }
      reader.join();
      EXPECT_EQ(observed, 3);
      EXPECT_EQ(path.GetPoint(path.CountPoints() - 1), (Point{5, 6, 0, 1}));
    }
  }
}

}  // namespace
}  // namespace skity
