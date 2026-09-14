// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <random>
#include <skity/geometry/stroke.hpp>
#include <skity/graphic/path.hpp>
#include <vector>

#include "gtest/gtest.h"
#include "src/geometry/geometry.hpp"
#include "src/geometry/math.hpp"
#include "src/geometry/point_priv.hpp"
#include "src/graphic/path_priv.hpp"

TEST(Path, test_iter) {
  skity::Path p;
  skity::Point pts[4];

  skity::Path::Iter no_path_iter;
  EXPECT_EQ(no_path_iter.Next(pts), skity::Path::Verb::kDone);

  no_path_iter.SetPath(p, false);
  EXPECT_EQ(no_path_iter.Next(pts), skity::Path::Verb::kDone);

  no_path_iter.SetPath(p, true);
  EXPECT_EQ(no_path_iter.Next(pts), skity::Path::Verb::kDone);

  skity::Path::Iter iter{p, false};
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kDone);

  p.Reset();
  iter.SetPath(p, false);
  EXPECT_FALSE(iter.IsClosedContour());

  p.LineTo(1, 1);
  p.Close();
  iter.SetPath(p, false);
  EXPECT_TRUE(iter.IsClosedContour());

  p.Reset();
  iter.SetPath(p, true);
  EXPECT_FALSE(iter.IsClosedContour());
  p.LineTo(1, 1);
  iter.SetPath(p, true);
  EXPECT_TRUE(iter.IsClosedContour());
  p.MoveTo(0, 0);
  p.LineTo(2, 2);
  iter.SetPath(p, false);
  EXPECT_FALSE(iter.IsClosedContour());

  p.Reset();
  p.QuadTo(0, 0, 0, 0);
  iter.SetPath(p, false);
  iter.Next(pts);
  EXPECT_EQ(skity::Path::Verb::kQuad, iter.Next(pts));

  p.Reset();
  p.ConicTo(0, 0, 0, 0, 0.5f);
  iter.SetPath(p, false);
  iter.Next(pts);
  EXPECT_EQ(skity::Path::Verb::kConic, iter.Next(pts));
}

static void check_move(skity::Path::RawIter* iter, float x0, float y0) {
  skity::Point pts[4];
  auto v = iter->Next(pts);
  EXPECT_EQ(v, skity::Path::Verb::kMove);
  EXPECT_FLOAT_EQ(pts[0].x, x0);
  EXPECT_FLOAT_EQ(pts[0].y, y0);
}

static void check_line(skity::Path::RawIter* iter, float x1, float y1) {
  skity::Point pts[4];
  auto v = iter->Next(pts);
  EXPECT_EQ(v, skity::Path::Verb::kLine);
  EXPECT_FLOAT_EQ(pts[1].x, x1);
  EXPECT_FLOAT_EQ(pts[1].y, y1);
}

static void check_quad(skity::Path::RawIter* iter, float x1, float y1, float x2,
                       float y2) {
  skity::Point pts[4];
  auto v = iter->Next(pts);
  EXPECT_EQ(v, skity::Path::Verb::kQuad);
  EXPECT_FLOAT_EQ(pts[1].x, x1);
  EXPECT_FLOAT_EQ(pts[1].y, y1);
  EXPECT_FLOAT_EQ(pts[2].x, x2);
  EXPECT_FLOAT_EQ(pts[2].y, y2);
}

static void check_done(skity::Path*, skity::Path::RawIter* iter) {
  skity::Point pts[4];
  auto v = iter->Next(pts);
  EXPECT_EQ(v, skity::Path::Verb::kDone);
}

static void check_done_and_reset(skity::Path* path,
                                 skity::Path::RawIter* iter) {
  check_done(path, iter);
  path->Reset();
}

static void check_path_is_line_and_reset(skity::Path* path, float x1,
                                         float y1) {
  skity::Path::RawIter iter(*path);
  check_move(std::addressof(iter), 0, 0);
  check_line(std::addressof(iter), x1, y1);
  check_done_and_reset(path, std::addressof(iter));
}

#if 0
static void check_path_is_line(skity::Path *path, float x1, float y1) {
  skity::Path::RawIter iter(*path);
  check_move(std::addressof(iter), 0, 0);
  check_line(std::addressof(iter), x1, y1);
  check_done(path, std::addressof(iter));
}
#endif

static void check_path_is_line_pair_and_reset(skity::Path* path, float x1,
                                              float y1, float x2, float y2) {
  skity::Path::RawIter iter(*path);
  check_move(std::addressof(iter), 0, 0);
  check_line(std::addressof(iter), x1, y1);
  check_line(std::addressof(iter), x2, y2);
  check_done_and_reset(path, std::addressof(iter));
}

static void check_path_is_quad_and_reset(skity::Path* path, float x1, float y1,
                                         float x2, float y2) {
  skity::Path::RawIter iter(*path);
  check_move(std::addressof(iter), 0, 0);
  check_quad(std::addressof(iter), x1, y1, x2, y2);
  check_done_and_reset(path, std::addressof(iter));
}

static void check_Close(const skity::Path& path) {
  for (int i = 0; i < 2; i++) {
    skity::Path::Iter iter(path, static_cast<bool>(i));
    skity::Point mv;
    skity::Point pts[4];
    skity::Path::Verb v;
    int nMT = 0;
    int nCL = 0;
    skity::PointSet(mv, 0, 0);
    while ((v = iter.Next(pts)) != skity::Path::Verb::kDone) {
      switch (v) {
        case skity::Path::Verb::kMove:
          mv = pts[0];
          ++nMT;
          break;
        case skity::Path::Verb::kClose:
          EXPECT_EQ(mv, pts[0]);
          ++nCL;
          break;
        default:
          break;
      }
    }
    EXPECT_TRUE(!i || nMT == nCL);
  }
}

TEST(Path, test_Close) {
  skity::Path ClosePt;
  ClosePt.MoveTo(0, 0);
  ClosePt.Close();
  check_Close(ClosePt);

  skity::Path openPt;
  openPt.MoveTo(0, 0);
  check_Close(openPt);

  skity::Path empty;
  check_Close(empty);
  empty.Close();
  check_Close(empty);

  skity::Path quad;
  quad.QuadTo(Float1, Float1, 10 * Float1, 10 * Float1);
  check_Close(quad);
  quad.Close();
  check_Close(quad);

  skity::Path cubic;
  cubic.CubicTo(Float1, Float1, 10 * Float1, 10 * Float1, 20 * Float1,
                20 * Float1);
  check_Close(cubic);
  cubic.Close();
  check_Close(cubic);

  skity::Path line;
  line.MoveTo(Float1, Float1);
  line.LineTo(10 * Float1, 10 * Float1);
  check_Close(line);
  line.Close();
  check_Close(line);

  skity::Path moves;
  moves.MoveTo(Float1, Float1);
  moves.MoveTo(5 * Float1, 5 * Float1);
  moves.MoveTo(Float1, 10 * Float1);
  moves.MoveTo(10 * Float1, Float1);
  check_Close(moves);
}

TEST(Path, test_ArcTo) {
  skity::Path p;

  p.ArcTo(0, 0, 1, 2, 1);
  check_path_is_line_and_reset(std::addressof(p), 0, 0);
  p.ArcTo(1, 2, 1, 2, 1);
  check_path_is_line_and_reset(std::addressof(p), 1, 2);
  p.ArcTo(1, 2, 3, 4, 0);
  check_path_is_line_and_reset(std::addressof(p), 1, 2);
  p.ArcTo(1, 2, 0, 0, 1);
  check_path_is_line_and_reset(std::addressof(p), 1, 2);
  p.ArcTo(1, 0, 1, 1, 1);
  skity::Point pt;
  EXPECT_TRUE(p.GetLastPt(std::addressof(pt)));
  EXPECT_FLOAT_EQ(pt.x, 1.f);
  EXPECT_FLOAT_EQ(pt.y, 1.f);
  p.Reset();
  p.ArcTo(1, 0, 1, -1, 1);
  EXPECT_TRUE(p.GetLastPt(std::addressof(pt)));
  EXPECT_FLOAT_EQ(pt.x, 1.f);
  EXPECT_FLOAT_EQ(pt.y, -1.f);
  {
    p.Reset();
    p.MoveTo(216, 216);
    // FIXME ArcTo is not correct
    // p.ArcTo(216, 108, 0, skity::Path::ArcSize::kLarge,
    //         skity::Path::Direction::kCW, 216, 0);
    // p.ArcTo(270, 135, 0, skity::Path::ArcSize::kLarge,
    //         skity::Path::Direction::kCCW, 216, 216);
    int n = p.CountPoints();
    EXPECT_EQ(p.GetPoint(0), p.GetPoint(n - 1));
  }
}

TEST(Path, test_quad) {
  skity::Path p;
  p.ConicTo(1, 2, 3, 4, -1);
  check_path_is_line_and_reset(std::addressof(p), 3, 4);
  p.ConicTo(1.f, 2.f, 3.f, 4.f, FloatInfinity);
  check_path_is_line_pair_and_reset(std::addressof(p), 1, 2, 3, 4);
  p.ConicTo(1, 2, 3, 4, 1);
  check_path_is_quad_and_reset(std::addressof(p), 1, 2, 3, 4);
}

TEST(Path_RawIter, test_RawIter) {
  skity::Path p;
  skity::Point pts[4];

  skity::Path::RawIter noPathIter;
  EXPECT_EQ(noPathIter.Next(pts), skity::Path::Verb::kDone);
  noPathIter.SetPath(p);
  EXPECT_EQ(noPathIter.Next(pts), skity::Path::Verb::kDone);

  skity::Path::RawIter iter(p);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kDone);

  p.MoveTo(Float1, 0);
  iter.SetPath(p);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kMove);
  EXPECT_FLOAT_EQ(pts[0].x, Float1);
  EXPECT_FLOAT_EQ(pts[0].y, 0);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kDone);

  // Only the last MoveTo is valid
  p.MoveTo(Float1 * 2, Float1);
  p.MoveTo(Float1 * 3, Float1 * 2);
  iter.SetPath(p);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kMove);
  EXPECT_FLOAT_EQ(pts[0].x, Float1 * 3);
  EXPECT_FLOAT_EQ(pts[0].y, Float1 * 2);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kDone);

  p.Reset();
  p.Close();
  iter.SetPath(p);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kDone);

  // Move/Close sequences
  p.Reset();
  p.Close();
  p.MoveTo(Float1, 0);
  p.Close();
  p.Close();
  p.MoveTo(Float1 * 2, Float1);
  p.Close();
  p.MoveTo(Float1 * 3, Float1 * 2);
  p.MoveTo(Float1 * 4, Float1 * 3);
  p.Close();
  iter.SetPath(p);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kMove);
  EXPECT_FLOAT_EQ(pts[0].x, Float1);
  EXPECT_FLOAT_EQ(pts[0].y, 0);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kClose);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kMove);
  EXPECT_FLOAT_EQ(pts[0].x, Float1 * 2);
  EXPECT_FLOAT_EQ(pts[0].y, Float1);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kClose);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kMove);
  EXPECT_FLOAT_EQ(pts[0].x, Float1 * 4);
  EXPECT_FLOAT_EQ(pts[0].y, Float1 * 3);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kClose);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kDone);

  // Generate random paths and verify
  skity::Point randomPts[25];
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      skity::PointSet(randomPts[i * 5 + j], Float1 * float(i),
                      Float1 * float(j));
    }
  }

  std::vector<skity::Path::Verb> verbs = {
      skity::Path::Verb::kMove,  skity::Path::Verb::kLine,
      skity::Path::Verb::kQuad,  skity::Path::Verb::kConic,
      skity::Path::Verb::kCubic, skity::Path::Verb::kClose};
  std::vector<skity::Point> expectedPts(31);
  std::vector<skity::Path::Verb> expectedVerbs(22);
  skity::Path::Verb nextVerb;

  for (int i = 0; i < 2; i++) {
    p.Reset();
    bool lastWasClose = true;
    bool haveMoteTo = false;
    bool lastWasMove = false;
    skity::Point lastMoveToPt = {0, 0, 0, 1};
    int numPoints = 0;
    std::random_device
        rd;  // Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd());  // Standard mersenne_twister_engine seeded with
                             // rd()
    int numVerbs = std::uniform_int_distribution<int>{0, 9}(gen);
    int numIterVerbs = 0;
    for (int j = 0; j < numVerbs; j++) {
      do {
        nextVerb =
            verbs[std::uniform_int_distribution<int>(0, verbs.size() - 1)(gen)];
      } while ((lastWasClose && nextVerb == skity::Path::Verb::kClose) ||
               (lastWasMove && nextVerb == skity::Path::Verb::kMove));
      switch (nextVerb) {
        case skity::Path::Verb::kMove:
          lastMoveToPt = expectedPts[numPoints] =
              randomPts[std::uniform_int_distribution<int>(0, 24)(gen)];
          p.MoveTo(expectedPts[numPoints].x, expectedPts[numPoints].y);
          numPoints += 1;
          lastWasClose = false;
          lastWasMove = true;
          haveMoteTo = true;
          break;
        case skity::Path::Verb::kLine:
          if (!haveMoteTo) {
            expectedPts[numPoints++] = lastMoveToPt;
            expectedVerbs[numIterVerbs++] = skity::Path::Verb::kMove;
            haveMoteTo = true;
          }
          expectedPts[numPoints] =
              randomPts[std::uniform_int_distribution<int>(0, 24)(gen)];
          p.LineTo(expectedPts[numPoints].x, expectedPts[numPoints].y);
          numPoints += 1;
          lastWasClose = false;
          lastWasMove = false;
          break;
        case skity::Path::Verb::kQuad:
          if (!haveMoteTo) {
            expectedPts[numPoints++] = lastMoveToPt;
            expectedVerbs[numIterVerbs++] = skity::Path::Verb::kMove;
            haveMoteTo = true;
          }
          expectedPts[numPoints] =
              randomPts[std::uniform_int_distribution<int>(0, 24)(gen)];
          expectedPts[numPoints + 1] =
              randomPts[std::uniform_int_distribution<int>(0, 24)(gen)];
          p.QuadTo(expectedPts[numPoints].x, expectedPts[numPoints].y,
                   expectedPts[numPoints + 1].x, expectedPts[numPoints + 1].y);
          numPoints += 2;
          lastWasClose = false;
          lastWasMove = false;
          break;
        case skity::Path::Verb::kConic:
          if (!haveMoteTo) {
            expectedPts[numPoints++] = lastMoveToPt;
            expectedVerbs[numIterVerbs++] = skity::Path::Verb::kMove;
            haveMoteTo = true;
          }
          expectedPts[numPoints] =
              randomPts[std::uniform_int_distribution<int>(0, 24)(gen)];
          expectedPts[numPoints + 1] =
              randomPts[std::uniform_int_distribution<int>(0, 24)(gen)];
          p.ConicTo(
              expectedPts[numPoints].x, expectedPts[numPoints].y,
              expectedPts[numPoints + 1].x, expectedPts[numPoints + 1].y,
              std::uniform_real_distribution<float>(0.f, 0.999f)(gen) * 4);
          numPoints += 2;
          lastWasClose = false;
          lastWasMove = false;
          break;
        case skity::Path::Verb::kCubic:
          if (!haveMoteTo) {
            expectedPts[numPoints++] = lastMoveToPt;
            expectedVerbs[numIterVerbs++] = skity::Path::Verb::kMove;
            haveMoteTo = true;
          }
          expectedPts[numPoints] =
              randomPts[std::uniform_int_distribution<int>(0, 24)(gen)];
          expectedPts[numPoints + 1] =
              randomPts[std::uniform_int_distribution<int>(0, 24)(gen)];
          expectedPts[numPoints + 2] =
              randomPts[std::uniform_int_distribution<int>(0, 24)(gen)];
          p.CubicTo(expectedPts[numPoints].x, expectedPts[numPoints].y,
                    expectedPts[numPoints + 1].x, expectedPts[numPoints + 1].y,
                    expectedPts[numPoints + 2].x, expectedPts[numPoints + 2].y);
          numPoints += 3;
          lastWasClose = false;
          lastWasMove = false;
          break;
        case skity::Path::Verb::kClose:
          p.Close();
          haveMoteTo = false;
          lastWasClose = true;
          lastWasMove = false;
          break;
        default:
          break;
      }
      expectedVerbs[numIterVerbs++] = nextVerb;
    }

    iter.SetPath(p);
    numVerbs = numIterVerbs;
    numIterVerbs = 0;
    int numIterPts = 0;

    skity::Point lastMoveTo;
    skity::Point lastPt;
    skity::PointSet(lastMoveTo, 0, 0);
    skity::PointSet(lastPt, 0, 0);
    while ((nextVerb = iter.Next(pts)) != skity::Path::Verb::kDone) {
      EXPECT_EQ(nextVerb, expectedVerbs[numIterVerbs]);
      numIterVerbs++;
      switch (nextVerb) {
        case skity::Path::Verb::kMove:
          EXPECT_TRUE(numIterPts < numPoints);
          EXPECT_FLOAT_EQ(pts[0].x, expectedPts[numIterPts].x);
          EXPECT_FLOAT_EQ(pts[0].y, expectedPts[numIterPts].y);
          lastPt = lastMoveTo = pts[0];
          numIterPts += 1;
          break;
        case skity::Path::Verb::kLine:
          EXPECT_LT(numIterPts, numPoints + 1);
          EXPECT_EQ(pts[0], lastPt);
          EXPECT_EQ(pts[1], expectedPts[numIterPts]);
          lastPt = pts[1];
          numIterPts += 1;
          break;
        case skity::Path::Verb::kQuad:
        case skity::Path::Verb::kConic:
          EXPECT_LT(numIterPts, numPoints + 2);
          EXPECT_EQ(pts[0], lastPt);
          EXPECT_EQ(pts[1], expectedPts[numIterPts]);
          EXPECT_EQ(pts[2], expectedPts[numIterPts + 1]);
          lastPt = pts[2];
          numIterPts += 2;
          break;
        case skity::Path::Verb::kCubic:
          EXPECT_LT(numIterPts, numPoints + 3);
          EXPECT_EQ(pts[0], lastPt);
          EXPECT_EQ(pts[1], expectedPts[numIterPts]);
          EXPECT_EQ(pts[2], expectedPts[numIterPts + 1]);
          EXPECT_EQ(pts[3], expectedPts[numIterPts + 2]);
          lastPt = pts[3];
          numIterPts += 3;
          break;
        case skity::Path::Verb::kClose:
          lastPt = lastMoveTo;
          break;
        default:
          break;
      }
    }
    EXPECT_EQ(numIterPts, numPoints);
    EXPECT_EQ(numIterVerbs, numVerbs);
  }
}

TEST(Path, bad_case) {
  skity::Path path;
  skity::Point randomPts[25];
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      skity::PointSet(randomPts[i * 5 + j], Float1 * float(i),
                      Float1 * float(j));
    }
  }

  path.MoveTo(randomPts[0].x, randomPts[0].y);
  path.CubicTo(randomPts[1].x, randomPts[1].y, randomPts[2].x, randomPts[2].y,
               randomPts[3].x, randomPts[3].y);
  path.CubicTo(randomPts[3].x, randomPts[3].y, randomPts[5].x, randomPts[5].y,
               randomPts[6].x, randomPts[6].y);
  path.Close();
  path.MoveTo(randomPts[7].x, randomPts[7].y);
  skity::Path::RawIter iter(path);

  skity::Point pts[4];
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kMove);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kCubic);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kCubic);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kClose);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kMove);
  EXPECT_EQ(pts[0], randomPts[7]);
}

TEST(path, test_range_iter) {
  skity::Path path;
  skity::PathPriv::Iterate iterate{path};

  EXPECT_TRUE(iterate.begin() == iterate.end());

  path.MoveTo(Float1, 0);
  iterate = skity::PathPriv::Iterate(path);

  auto iter = iterate.begin();
  {
    auto ret = *iter++;
    auto verb = std::get<0>(ret);
    auto pts = std::get<1>(ret);
    EXPECT_EQ(verb, skity::Path::Verb::kMove);
    EXPECT_EQ(pts[0].x, Float1);
    EXPECT_EQ(pts[0].y, 0.f);
  }
  EXPECT_TRUE(iter == iterate.end());

  path.LineTo(Float1 * 2, Float1);
  path.LineTo(Float1 * 3, Float1 * 2);
  iterate = skity::PathPriv::Iterate(path);
  iter = iterate.begin();
  {
    auto ret = *iter++;
    auto verb = std::get<0>(ret);
    auto pts = std::get<1>(ret);
    EXPECT_EQ(verb, skity::Path::Verb::kMove);
    EXPECT_EQ(pts[0].x, Float1);
    EXPECT_EQ(pts[0].y, 0.f);
  }
  {
    auto ret = *iter++;
    auto verb = std::get<0>(ret);
    auto pts = std::get<1>(ret);
    EXPECT_EQ(verb, skity::Path::Verb::kLine);
    EXPECT_EQ(pts[1].x, Float1 * 2);
    EXPECT_EQ(pts[1].y, Float1);
  }
  {
    auto ret = *iter++;
    auto verb = std::get<0>(ret);
    auto pts = std::get<1>(ret);
    EXPECT_EQ(verb, skity::Path::Verb::kLine);
    EXPECT_EQ(pts[1].x, Float1 * 3);
    EXPECT_EQ(pts[1].y, Float1 * 2);
  }

  EXPECT_TRUE(iter == iterate.end());

  path.Reset();
  path.Close();
  iterate = skity::PathPriv::Iterate(path);
  EXPECT_TRUE(iterate.begin() == iterate.end());

  path.Reset();
  path.Close();  // Not stored, no purpose
  path.MoveTo(Float1, 0);
  path.Close();
  path.Close();  // Not stored, no purpose
  path.MoveTo(Float1 * 2, Float1);
  path.Close();
  path.MoveTo(Float1 * 3, Float1 * 2);
  path.MoveTo(Float1 * 4, Float1 * 3);
  path.Close();

  iterate = skity::PathPriv::Iterate(path);
  iter = iterate.begin();
  {
    auto ret = *iter++;
    auto verb = std::get<0>(ret);
    auto pts = std::get<1>(ret);
    EXPECT_EQ(verb, skity::Path::Verb::kMove);
    EXPECT_EQ(pts[0].x, Float1);
    EXPECT_EQ(pts[0].y, 0);
  }
  {
    auto ret = *iter++;
    auto verb = std::get<0>(ret);
    EXPECT_EQ(verb, skity::Path::Verb::kClose);
  }
  {
    auto ret = *iter++;
    auto verb = std::get<0>(ret);
    auto pts = std::get<1>(ret);
    EXPECT_EQ(verb, skity::Path::Verb::kMove);
    EXPECT_EQ(pts[0].x, Float1 * 2);
    EXPECT_EQ(pts[0].y, Float1);
  }
  {
    auto ret = *iter++;
    auto verb = std::get<0>(ret);
    EXPECT_EQ(verb, skity::Path::Verb::kClose);
  }
  {
    auto ret = *iter++;
    auto verb = std::get<0>(ret);
    auto pts = std::get<1>(ret);
    EXPECT_EQ(verb, skity::Path::Verb::kMove);
    EXPECT_EQ(pts[0].x, Float1 * 4);
    EXPECT_EQ(pts[0].y, Float1 * 3);
  }
  {
    auto ret = *iter++;
    auto verb = std::get<0>(ret);
    EXPECT_EQ(verb, skity::Path::Verb::kClose);
  }
  EXPECT_TRUE(iter == iterate.end());
}

struct IsRectTest {
  skity::Vec2* points;
  size_t count;
  bool Close;
  bool is_rect;
};

TEST(path, IsRect) {
  // passing tests
  std::vector<skity::Vec2> r1{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  std::vector<skity::Vec2> r2{{1, 0}, {1, 1}, {0, 1}, {0, 0}};
  std::vector<skity::Vec2> r3{{1, 1}, {0, 1}, {0, 0}, {1, 0}};
  std::vector<skity::Vec2> r4{{0, 1}, {0, 0}, {1, 0}, {1, 1}};
  std::vector<skity::Vec2> r5{{0, 0}, {0, 1}, {1, 1}, {1, 0}};
  std::vector<skity::Vec2> r6{{0, 1}, {1, 1}, {1, 0}, {0, 0}};
  std::vector<skity::Vec2> r7{{1, 1}, {1, 0}, {0, 0}, {0, 1}};
  std::vector<skity::Vec2> r8{{1, 0}, {0, 0}, {0, 1}, {1, 1}};
  std::vector<skity::Vec2> r9{{0, 1}, {1, 1}, {1, 0}, {0, 0}};
  std::vector<skity::Vec2> ra{{0, 0}, {0, .5f}, {0, 1}, {.5f, 1},
                              {1, 1}, {1, .5f}, {1, 0}, {.5f, 0}};
  std::vector<skity::Vec2> rb{{0, 0}, {.5f, 0}, {1, 0}, {1, .5f},
                              {1, 1}, {.5f, 1}, {0, 1}, {0, .5f}};
  std::vector<skity::Vec2> rc{{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}};
  std::vector<skity::Vec2> rd{{0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}};
  std::vector<skity::Vec2> re{{0, 0}, {1, 0}, {1, 0}, {1, 1}, {0, 1}};
  std::vector<skity::Vec2> rf{{1, 0}, {8, 0}, {8, 8}, {0, 8}, {0, 0}};

  // failing tests
  std::vector<skity::Vec2> f1{{0, 0}, {1, 0}, {1, 1}};  // too few points
  std::vector<skity::Vec2> f2{{0, 0}, {1, 1}, {0, 1}, {1, 0}};  // diagonal
  std::vector<skity::Vec2> f3{{0, 0}, {1, 0}, {1, 1},
                              {0, 1}, {0, 0}, {1, 0}};  // wraps
  std::vector<skity::Vec2> f4{{0, 0}, {1, 0}, {0, 0},
                              {1, 0}, {1, 1}, {0, 1}};  // backs up
  std::vector<skity::Vec2> f5{
      {0, 0}, {1, 0}, {1, 1}, {2, 0}};  // end overshoots
  std::vector<skity::Vec2> f6{
      {0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 2}};  // end overshoots
  std::vector<skity::Vec2> f7{
      {0, 0}, {1, 0}, {1, 1}, {0, 2}};  // end overshoots
  std::vector<skity::Vec2> f8{{0, 0}, {1, 0}, {1, 1}, {1, 0}};  // 'L'
  std::vector<skity::Vec2> f9{{1, 0}, {8, 0}, {8, 8},
                              {0, 8}, {0, 0}, {2, 0}};  // overlaps
  std::vector<skity::Vec2> fa{{1, 0}, {8, 0},  {8, 8},
                              {0, 8}, {0, -1}, {1, -1}};  // non colinear gap
  std::vector<skity::Vec2> fb{
      {1, 0}, {8, 0}, {8, 8}, {0, 8}, {0, 1}};  // falls short

  // no Close, but we should detect them as fillably the same as a rect
  std::vector<skity::Vec2> c1{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  std::vector<skity::Vec2> c2{{0, 0}, {1, 0}, {1, 2}, {0, 2}, {0, 1}};
  std::vector<skity::Vec2> c3{{0, 0}, {1, 0}, {1, 2},
                              {0, 2}, {0, 1}, {0, 0}};  // hit the start

  // like c2, but we double-back on ourselves
  std::vector<skity::Vec2> d1{{0, 0}, {1, 0}, {1, 2}, {0, 2}, {0, 1}, {0, 2}};
  // like c2, but we overshoot the start point
  std::vector<skity::Vec2> d2{{0, 0}, {1, 0}, {1, 2}, {0, 2}, {0, -1}};
  std::vector<skity::Vec2> d3{{0, 0}, {1, 0}, {1, 2}, {0, 2}, {0, -1}, {0, 0}};

  std::vector<IsRectTest> tests{
      {r1.data(), r1.size(), true, true},   {r2.data(), r2.size(), true, true},
      {r3.data(), r3.size(), true, true},   {r4.data(), r4.size(), true, true},
      {r5.data(), r5.size(), true, true},   {r6.data(), r6.size(), true, true},
      {r7.data(), r7.size(), true, true},   {r8.data(), r8.size(), true, true},
      {r9.data(), r9.size(), true, true},   {ra.data(), ra.size(), true, true},
      {rb.data(), rb.size(), true, true},   {rc.data(), rc.size(), true, true},
      {rd.data(), rd.size(), true, true},   {re.data(), re.size(), true, true},
      {rf.data(), rf.size(), true, true},

      {f1.data(), f1.size(), true, false},  {f2.data(), f2.size(), true, false},
      {f3.data(), f3.size(), true, false},  {f4.data(), f4.size(), true, false},
      {f5.data(), f5.size(), true, false},  {f6.data(), f6.size(), true, false},
      {f7.data(), f7.size(), true, false},  {f8.data(), f8.size(), true, false},
      {f9.data(), f9.size(), true, false},  {fa.data(), fa.size(), true, false},
      {fb.data(), fb.size(), true, false},

      {c1.data(), c1.size(), false, true},  {c2.data(), c2.size(), false, true},
      {c3.data(), c3.size(), false, true},

      {d1.data(), d1.size(), false, false}, {d2.data(), d2.size(), false, true},
      {d3.data(), d3.size(), false, false},
  };

  int32_t index = 0;
  std::for_each(tests.begin(), tests.end(), [&index](IsRectTest const& test) {
    skity::Path path;
    path.MoveTo(test.points[0].x, test.points[0].y);

    for (size_t i = 1; i < test.count; i++) {
      path.LineTo(test.points[i].x, test.points[i].y);
    }

    if (test.Close) {
      path.Close();
    }

    EXPECT_TRUE(test.is_rect == path.IsRect(nullptr))
        << "failed index = " << index;
    index++;
  });

  // fail, Close then line
  skity::Path path1;
  path1.MoveTo(r1[0].x, r1[0].y);
  for (size_t i = 0; i < r1.size() - 1; i++) {
    path1.LineTo(r1[i].x, r1[i].y);
  }
  path1.Close();
  path1.LineTo(1, 0);
  EXPECT_FALSE(path1.IsRect(nullptr));

  // fail, move in the middle
  path1.Reset();
  path1.MoveTo(r1[0].x, r1[0].y);
  for (size_t i = 0; i < r1.size(); i++) {
    if (i == 2) {
      path1.MoveTo(1, .5f);
    }

    path1.LineTo(r1[i].x, r1[i].y);
  }
  path1.Close();
  EXPECT_FALSE(path1.IsRect(nullptr));

  // fail, move on the edge
  path1.Reset();
  path1.MoveTo(r1[0].x, r1[0].y);
  for (size_t i = 1; i < r1.size(); i++) {
    path1.MoveTo(r1[i - 1].x, r1[i - 1].y);
    path1.LineTo(r1[i].x, r1[i].y);
  }
  path1.Close();
  EXPECT_FALSE(path1.IsRect(nullptr));

  // fail, quad
  path1.Reset();
  path1.MoveTo(r1[0].x, r1[0].y);
  for (size_t i = 0; i < r1.size(); i++) {
    if (i == 2) {
      path1.QuadTo(1, .5f, 1, .5f);
    }
    path1.LineTo(r1[i].x, r1[i].y);
  }
  path1.Close();
  EXPECT_FALSE(path1.IsRect(nullptr));
}

TEST(Path, Contains) {
  skity::Path path;

  EXPECT_FALSE(path.Contains(0, 0));

  path.MoveTo(4, 4);
  path.LineTo(6, 8);
  path.LineTo(6, 2);
  path.SetFillType(skity::Path::PathFillType::kEvenOdd);
  EXPECT_TRUE(path.Contains(5, 4));
  EXPECT_TRUE(path.Contains(6, 8));
  EXPECT_FALSE(path.Contains(8, 8));

  path.Reset();
  path.MoveTo(8, 6);
  path.LineTo(7, 7);
  path.LineTo(8, 8);
  path.LineTo(9, 7);
  path.SetFillType(skity::Path::PathFillType::kEvenOdd);
  EXPECT_TRUE(path.Contains(8, 7));
  EXPECT_TRUE(path.Contains(9, 7));
  EXPECT_FALSE(path.Contains(10, 7));

  path.Reset();
  path.MoveTo(10, 6);
  path.LineTo(6, 7);
  path.LineTo(10, 8);
  path.LineTo(8, 7);
  path.SetFillType(skity::Path::PathFillType::kEvenOdd);
  EXPECT_TRUE(path.Contains(7, 7));
  EXPECT_FALSE(path.Contains(9, 7));

  skity::Path ring;
  ring.MoveTo(8, 6);
  ring.LineTo(7, 7);
  ring.LineTo(8, 8);
  ring.LineTo(9, 7);
  ring.LineTo(8, 6);

  ring.MoveTo(8, 4);
  ring.LineTo(5, 7);
  ring.LineTo(8, 12);
  ring.LineTo(12, 7);
  ring.Close();
  EXPECT_TRUE(ring.Contains(8, 7));
  ring.SetFillType(skity::Path::PathFillType::kEvenOdd);
  EXPECT_FALSE(ring.Contains(8, 7));
  EXPECT_TRUE(ring.Contains(8, 6));
  EXPECT_TRUE(ring.Contains(9, 7));
  EXPECT_TRUE(ring.Contains(6, 7));
  EXPECT_TRUE(ring.Contains(10, 7));

  path.Reset();
  path.MoveTo(4, 4);
  path.LineTo(6, 8);
  path.LineTo(8, 4);

  // test on edge
  EXPECT_TRUE(path.Contains(6, 4));
  EXPECT_TRUE(path.Contains(5, 6));
  EXPECT_TRUE(path.Contains(7, 6));
  // quick reject
  EXPECT_FALSE(path.Contains(4, 0));
  EXPECT_FALSE(path.Contains(0, 4));
  EXPECT_FALSE(path.Contains(4, 10));
  EXPECT_FALSE(path.Contains(10, 4));
  // test various in x;
  EXPECT_FALSE(path.Contains(5, 7));
  EXPECT_TRUE(path.Contains(6, 7));
  EXPECT_FALSE(path.Contains(7, 7));

  path.Reset();
  path.MoveTo(4, 4);
  path.LineTo(8, 6);
  path.LineTo(4, 8);

  // test on edge
  EXPECT_TRUE(path.Contains(4, 6));
  EXPECT_TRUE(path.Contains(6, 5));
  EXPECT_TRUE(path.Contains(6, 7));
  // test various crossings in y
  EXPECT_FALSE(path.Contains(7, 5));
  EXPECT_TRUE(path.Contains(7, 6));
  EXPECT_FALSE(path.Contains(7, 7));

  path.Reset();
  path.MoveTo(4, 4);
  path.LineTo(8, 4);
  path.LineTo(8, 8);
  path.LineTo(4, 8);

  // test on vertices
  EXPECT_TRUE(path.Contains(4, 4));
  EXPECT_TRUE(path.Contains(8, 4));
  EXPECT_TRUE(path.Contains(8, 8));
  EXPECT_TRUE(path.Contains(4, 8));

  // test quads
  path.Reset();
  path.MoveTo(4, 4);
  path.QuadTo(6, 6, 8, 8);
  path.QuadTo(6, 8, 4, 8);
  path.QuadTo(4, 6, 4, 4);
  EXPECT_TRUE(path.Contains(5, 6));
  EXPECT_FALSE(path.Contains(6, 5));
  // test quad edge
  EXPECT_TRUE(path.Contains(5, 5));
  EXPECT_TRUE(path.Contains(5, 8));
  EXPECT_TRUE(path.Contains(4, 5));
  // test quad endpoints
  EXPECT_TRUE(path.Contains(4, 4));
  EXPECT_TRUE(path.Contains(8, 8));
  EXPECT_TRUE(path.Contains(4, 8));

  path.Reset();

  std::array<skity::Vec2, 7> pts{
      skity::Vec2{6, 6}, skity::Vec2{8, 8}, skity::Vec2{6, 8},
      skity::Vec2{4, 8}, skity::Vec2{4, 6}, skity::Vec2{4, 4},
      skity::Vec2{6, 6},
  };
  path.MoveTo(pts[0].x, pts[0].y);

  for (size_t i = 1; i < pts.size(); i += 2) {
    path.QuadTo(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y);
  }

  EXPECT_FALSE(path.Contains(4, 5));
  EXPECT_FALSE(path.Contains(5, 5));
  EXPECT_TRUE(path.Contains(4, 6));
  EXPECT_TRUE(path.Contains(5, 6));
  EXPECT_TRUE(path.Contains(6, 6));
  EXPECT_FALSE(path.Contains(7, 6));
  EXPECT_FALSE(path.Contains(4, 7));
  EXPECT_TRUE(path.Contains(5, 7));
  EXPECT_TRUE(path.Contains(6, 7));
  EXPECT_FALSE(path.Contains(7, 7));
  EXPECT_FALSE(path.Contains(4, 8));
  EXPECT_FALSE(path.Contains(5, 8));
  EXPECT_TRUE(path.Contains(6, 8));
  EXPECT_FALSE(path.Contains(7, 8));

  // test conics
  std::array<skity::Vec2, 10> c_pts{
      skity::Vec2{5, 4}, skity::Vec2{6, 5}, skity::Vec2{7, 6},
      skity::Vec2{6, 6}, skity::Vec2{4, 6}, skity::Vec2{5, 7},
      skity::Vec2{5, 5}, skity::Vec2{5, 4}, skity::Vec2{6, 5},
      skity::Vec2{7, 6},
  };

  for (int32_t i = 0; i < 3; i++) {
    path.Reset();
    path.SetFillType(skity::Path::PathFillType::kEvenOdd);

    path.MoveTo(c_pts[i].x, c_pts[i].y);
    path.CubicTo(c_pts[i + 1].x, c_pts[i + 1].y, c_pts[i + 2].x, c_pts[i + 2].y,
                 c_pts[i + 3].x, c_pts[i + 3].y);
    path.CubicTo(c_pts[i + 4].x, c_pts[i + 4].y, c_pts[i + 5].x, c_pts[i + 5].y,
                 c_pts[i + 6].x, c_pts[i + 6].y);
    path.Close();

    EXPECT_TRUE(path.Contains(5.5f, 5.5f));
    EXPECT_FALSE(path.Contains(4.5f, 5.5f));

    // test cubic end points
    EXPECT_TRUE(path.Contains(c_pts[i].x, c_pts[i].y));
    EXPECT_TRUE(path.Contains(c_pts[i + 3].x, c_pts[i + 3].y));
    EXPECT_TRUE(path.Contains(c_pts[i + 6].x, c_pts[i + 6].y));
  }
}

union FloatIntUnion {
  float fFloat;
  int32_t fSignBitInt;
};

// Helper to see a bit pattern as a float (w/o aliasing warnings)
static inline float Bits2Float(int32_t floatAsBits) {
  FloatIntUnion data;
  data.fSignBitInt = floatAsBits;
  return data.fFloat;
}

TEST(Path, Convexity1) {
  skity::Path path;
  EXPECT_TRUE(path.IsConvex());

  path.Reset();
  path.AddCircle(0, 0, 10);
  EXPECT_TRUE(path.IsConvex());

  path.Reset();
  path.AddCircle(0, 0, 10);
  path.AddCircle(0, 0, 10);
  EXPECT_FALSE(path.IsConvex());

  path.Reset();
  path.AddRect(0, 0, 10, 10, skity::Path::Direction::kCCW);
  EXPECT_TRUE(path.IsConvex());

  path.Reset();
  path.AddRect(0, 0, 10, 10, skity::Path::Direction::kCW);
  EXPECT_TRUE(path.IsConvex());

  path.Reset();
  path.QuadTo(100, 100, 50, 50);
  EXPECT_TRUE(path.IsConvex());

  path.Reset();
  path.MoveTo(0, 0);
  EXPECT_TRUE(path.IsConvex());

  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(10, 10);
  EXPECT_TRUE(path.IsConvex());

  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(10, 10);
  path.LineTo(20, 20);
  path.LineTo(0, 0);
  path.LineTo(10, 10);
  EXPECT_FALSE(path.IsConvex());

  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(10, 10);
  path.LineTo(10, 20);
  EXPECT_TRUE(path.IsConvex());

  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(10, 10);
  path.LineTo(10, 0);
  EXPECT_TRUE(path.IsConvex());

  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(10, 10);
  path.LineTo(10, 0);
  path.LineTo(0, 10);
  EXPECT_FALSE(path.IsConvex());

  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(10, 0);
  path.LineTo(0, 10);
  path.LineTo(-10, -10);
  EXPECT_FALSE(path.IsConvex());

  static const float float_max = 3.402823466e+38f;
  static const float float_min = -float_max;
  static const skity::Point axisAlignedPts[] = {
      {float_max, 0, 0, 1},
      {0, float_max, 0, 1},
      {float_min, 0, 0, 1},
      {0, float_min, 0, 1},
  };

  const size_t axisAlignedPtsCount =
      sizeof(axisAlignedPts) / sizeof(axisAlignedPts[0]);

  for (int index = 0; index < (int)(11 * axisAlignedPtsCount); ++index) {
    int f = (int)(index % axisAlignedPtsCount);
    int g = (int)((f + 1) % axisAlignedPtsCount);
    path.Reset();
    int curveSelect = index % 11;
    switch (curveSelect) {
      case 0:
        path.MoveTo(axisAlignedPts[f]);
        break;
      case 1:
        path.LineTo(axisAlignedPts[f]);
        break;
      case 2:
        path.QuadTo(axisAlignedPts[f], axisAlignedPts[f]);
        break;
      case 3:
        path.QuadTo(axisAlignedPts[f], axisAlignedPts[g]);
        break;
      case 4:
        path.QuadTo(axisAlignedPts[g], axisAlignedPts[f]);
        break;
      case 5:
        path.CubicTo(axisAlignedPts[f], axisAlignedPts[f], axisAlignedPts[f]);
        break;
      case 6:
        path.CubicTo(axisAlignedPts[f], axisAlignedPts[f], axisAlignedPts[g]);
        break;
      case 7:
        path.CubicTo(axisAlignedPts[f], axisAlignedPts[g], axisAlignedPts[f]);
        break;
      case 8:
        path.CubicTo(axisAlignedPts[f], axisAlignedPts[g], axisAlignedPts[g]);
        break;
      case 9:
        path.CubicTo(axisAlignedPts[g], axisAlignedPts[f], axisAlignedPts[f]);
        break;
      case 10:
        path.CubicTo(axisAlignedPts[g], axisAlignedPts[f], axisAlignedPts[g]);
        break;
    }
    if (curveSelect == 0 || curveSelect == 1 || curveSelect == 2 ||
        curveSelect == 5) {
      EXPECT_TRUE(path.IsConvex());
    } else {
      EXPECT_FALSE(path.IsConvex());
    }
  }

  path.Reset();
  path.MoveTo(Bits2Float(0xbe9171db),
              Bits2Float(0xbd7eeb5d));  // -0.284072f, -0.0622362f
  path.LineTo(Bits2Float(0xbe9171db),
              Bits2Float(0xbd7eea38));  // -0.284072f, -0.0622351f
  path.LineTo(Bits2Float(0xbe9171a0),
              Bits2Float(0xbd7ee5a7));  // -0.28407f, -0.0622307f
  path.LineTo(Bits2Float(0xbe917147),
              Bits2Float(0xbd7ed886));  // -0.284067f, -0.0622182f
  path.LineTo(Bits2Float(0xbe917378),
              Bits2Float(0xbd7ee1a9));  // -0.284084f, -0.0622269f
  path.LineTo(Bits2Float(0xbe9171db),
              Bits2Float(0xbd7eeb5d));  // -0.284072f, -0.0622362f
  path.Close();
  EXPECT_FALSE(path.IsConvex());
}

TEST(Path, Convexity2) {
  skity::Path pt;
  pt.MoveTo(0, 0);
  pt.Close();
  EXPECT_TRUE(pt.IsConvex());

  skity::Path line;
  line.MoveTo(12, 20);
  line.LineTo(-12, -20);
  line.Close();
  EXPECT_TRUE(line.IsConvex());

  skity::Path triLeft;
  triLeft.MoveTo(0, 0);
  triLeft.LineTo(1, 0);
  triLeft.LineTo(1, 1);
  triLeft.Close();
  EXPECT_TRUE(triLeft.IsConvex());

  skity::Path triRight;
  triRight.MoveTo(0, 0);
  triRight.LineTo(-1, 0);
  triRight.LineTo(1, 1);
  triRight.Close();
  EXPECT_TRUE(triRight.IsConvex());

  skity::Path square;
  square.MoveTo(0, 0);
  square.LineTo(1, 0);
  square.LineTo(1, 1);
  square.LineTo(0, 1);
  square.Close();
  EXPECT_TRUE(square.IsConvex());

  skity::Path redundantSquare;
  redundantSquare.MoveTo(0, 0);
  redundantSquare.LineTo(0, 0);
  redundantSquare.LineTo(0, 0);
  redundantSquare.LineTo(1, 0);
  redundantSquare.LineTo(1, 0);
  redundantSquare.LineTo(1, 0);
  redundantSquare.LineTo(1, 1);
  redundantSquare.LineTo(1, 1);
  redundantSquare.LineTo(1, 1);
  redundantSquare.LineTo(0, 1);
  redundantSquare.LineTo(0, 1);
  redundantSquare.LineTo(0, 1);
  redundantSquare.Close();
  EXPECT_TRUE(redundantSquare.IsConvex());

  skity::Path bowTie;
  bowTie.MoveTo(0, 0);
  bowTie.LineTo(0, 0);
  bowTie.LineTo(0, 0);
  bowTie.LineTo(1, 1);
  bowTie.LineTo(1, 1);
  bowTie.LineTo(1, 1);
  bowTie.LineTo(1, 0);
  bowTie.LineTo(1, 0);
  bowTie.LineTo(1, 0);
  bowTie.LineTo(0, 1);
  bowTie.LineTo(0, 1);
  bowTie.LineTo(0, 1);
  bowTie.Close();
  EXPECT_FALSE(bowTie.IsConvex());

  skity::Path spiral;
  spiral.MoveTo(0, 0);
  spiral.LineTo(100, 0);
  spiral.LineTo(100, 100);
  spiral.LineTo(0, 100);
  spiral.LineTo(0, 50);
  spiral.LineTo(50, 50);
  spiral.LineTo(50, 75);
  spiral.Close();
  EXPECT_FALSE(spiral.IsConvex());

  skity::Path dent;
  dent.MoveTo(0, 0);
  dent.LineTo(100, 100);
  dent.LineTo(0, 100);
  dent.LineTo(-50, 200);
  dent.LineTo(-200, 100);
  dent.Close();
  EXPECT_FALSE(dent.IsConvex());

  skity::Path strokedSin;
  for (int i = 0; i < 2000; i++) {
    float x = i / 2.0;
    float y = 500 - (x + std::sin(x / 100) * 40) / 3;
    if (0 == i) {
      strokedSin.MoveTo(x, y);
    } else {
      strokedSin.LineTo(x, y);
    }
  }
  skity::Paint paint;
  paint.SetStrokeWidth(2);
  skity::Stroke stroke(paint);
  skity::Path strokeDst;
  stroke.StrokePath(strokedSin, &strokeDst);
  EXPECT_FALSE(dent.IsConvex());

  skity::Path degenerateConcave;
  degenerateConcave.MoveTo(148.67912f, 191.875f);
  degenerateConcave.LineTo(470.37695f, 7.5f);
  degenerateConcave.LineTo(148.67912f, 191.875f);
  degenerateConcave.LineTo(41.446522f, 376.25f);
  degenerateConcave.LineTo(-55.971577f, 460.0f);
  degenerateConcave.LineTo(41.446522f, 376.25f);
  EXPECT_FALSE(degenerateConcave.IsConvex());

  skity::Path badFirstVector;
  badFirstVector.MoveTo(501.087708f, 319.610352f);
  badFirstVector.LineTo(501.087708f, 319.610352f);
  badFirstVector.CubicTo(501.087677f, 319.610321f, 449.271606f, 258.078674f,
                         395.084564f, 198.711182f);
  badFirstVector.CubicTo(358.967072f, 159.140717f, 321.910553f, 120.650436f,
                         298.442322f, 101.955399f);
  badFirstVector.LineTo(301.557678f, 98.044601f);
  badFirstVector.CubicTo(325.283844f, 116.945084f, 362.615204f, 155.720825f,
                         398.777557f, 195.340454f);
  badFirstVector.CubicTo(453.031860f, 254.781662f, 504.912262f, 316.389618f,
                         504.912292f, 316.389648f);
  badFirstVector.LineTo(504.912292f, 316.389648f);
  badFirstVector.LineTo(501.087708f, 319.610352f);
  badFirstVector.Close();
  EXPECT_FALSE(badFirstVector.IsConvex());

  skity::Path falseBackEdge;
  falseBackEdge.MoveTo(-217.83430557928145f, -382.14948768484857f);
  falseBackEdge.LineTo(-227.73867866614847f, -399.52485512718323f);
  falseBackEdge.CubicTo(-158.3541047666846f, -439.0757140459542f,
                        -79.8654464485281f, -459.875f, -1.1368683772161603e-13f,
                        -459.875f);
  falseBackEdge.LineTo(-8.08037266162413e-14f, -439.875f);
  falseBackEdge.LineTo(-8.526512829121202e-14f, -439.87499999999994f);
  falseBackEdge.CubicTo(-76.39209188702645f, -439.87499999999994f,
                        -151.46727226799754f, -419.98027663161537f,
                        -217.83430557928145f, -382.14948768484857f);
  falseBackEdge.Close();
  EXPECT_FALSE(falseBackEdge.IsConvex());
}

TEST(Path, ConvexityDoubleBack) {
  skity::Path doubleback;
  doubleback.LineTo(1, 1);
  EXPECT_TRUE(doubleback.IsConvex());
  doubleback.LineTo(2, 2);
  EXPECT_TRUE(doubleback.IsConvex());
  doubleback.Reset();
  doubleback.LineTo(1, 0);
  EXPECT_TRUE(doubleback.IsConvex());
  doubleback.LineTo(2, 0);
  EXPECT_TRUE(doubleback.IsConvex());
  doubleback.LineTo(1, 0);
  EXPECT_TRUE(doubleback.IsConvex());
  doubleback.Reset();
  doubleback.QuadTo(1, 1, 2, 2);
  EXPECT_TRUE(doubleback.IsConvex());
  doubleback.Reset();
  doubleback.QuadTo(1, 0, 2, 0);
  EXPECT_TRUE(doubleback.IsConvex());
  doubleback.QuadTo(1, 0, 0, 0);
  EXPECT_TRUE(doubleback.IsConvex());

  doubleback.Reset();
  doubleback.LineTo(1, 0);
  doubleback.LineTo(1, 0);
  doubleback.LineTo(1, 1);
  doubleback.LineTo(1, 1);
  doubleback.LineTo(1, 0);
  EXPECT_FALSE(doubleback.IsConvex());

  doubleback.Reset();
  doubleback.LineTo(-1, 0);
  doubleback.LineTo(-1, 1);
  doubleback.LineTo(-1, 0);
  EXPECT_FALSE(doubleback.IsConvex());
}

TEST(Path, AddPath) {
  skity::Path child;
  child.AddRect(skity::Rect::MakeLTRB(20, 20, 30, 40));
  skity::Path path;
  path.MoveTo(10, 10);
  path.LineTo(25, 10);
  path.AddPath(child);
  path.LineTo(100, 100);
  skity::Path::Iter iter;
  iter.SetPath(path, false);
  skity::Point pts[4];
  int move_count = 0;
  skity::Path::Verb v;
  while ((v = iter.Next(pts)) != skity::Path::Verb::kDone) {
    if (v == skity::Path::Verb::kMove) {
      move_count++;
    }
  }
  EXPECT_EQ(move_count, 3);
}

TEST(Path, StrokePath) {
  skity::Path src;
  src.MoveTo(30, 30);
  src.QuadTo(40, 30, 120, 30);
  skity::Paint paint;
  paint.SetStrokeWidth(2);
  skity::Stroke stroke(paint);
  skity::Path dst;
  stroke.StrokePath(src, &dst);
  EXPECT_TRUE(dst.Contains(120, 30));
}

TEST(Path, SegmentMask) {
  skity::Path path;
  EXPECT_EQ(path.GetSegmentMasks(), 0);
  path.MoveTo(0, 0);
  EXPECT_EQ(path.GetSegmentMasks(), 0);

  path.Reset();
  path.MoveTo(0, 0);
  path.MoveTo(1, 1);
  EXPECT_EQ(path.GetSegmentMasks(), 0);

  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(1, 1);
  EXPECT_EQ(path.GetSegmentMasks(), skity::Path::SegmentMask::kLine);

  path.Reset();
  path.MoveTo(0, 0);
  path.QuadTo(1, 1, 2, 2);
  EXPECT_EQ(path.GetSegmentMasks(), skity::Path::SegmentMask::kQuad);

  path.Reset();
  path.MoveTo(0, 0);
  path.ConicTo(1, 1, 2, 2, 0.5f);
  EXPECT_EQ(path.GetSegmentMasks(), skity::Path::SegmentMask::kConic);

  path.Reset();
  path.MoveTo(0, 0);
  path.CubicTo(1, 1, 2, 2, 3, 3);
  EXPECT_EQ(path.GetSegmentMasks(), skity::Path::SegmentMask::kCubic);

  path.Reset();
  path.MoveTo(0, 0);
  path.QuadTo(1, 1, 2, 2);
  path.CubicTo(3, 3, 4, 4, 5, 5);
  EXPECT_EQ(path.GetSegmentMasks(),
            skity::Path::SegmentMask::kQuad | skity::Path::SegmentMask::kCubic);

  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(1, 1);
  path.QuadTo(2, 2, 3, 3);
  path.ConicTo(4, 4, 5, 5, 0.5f);
  path.CubicTo(4, 4, 5, 5, 6, 6);
  EXPECT_EQ(path.GetSegmentMasks(), skity::Path::SegmentMask::kLine |
                                        skity::Path::SegmentMask::kQuad |
                                        skity::Path::SegmentMask::kConic |
                                        skity::Path::SegmentMask::kCubic);

  path.Reset();
  EXPECT_EQ(path.GetSegmentMasks(), 0);

  // Swap
  {
    skity::Path a;
    a.MoveTo(0, 0);
    a.LineTo(1, 1);
    EXPECT_EQ(a.GetSegmentMasks(), skity::Path::SegmentMask::kLine);
    skity::Path b;
    b.MoveTo(0, 0);
    b.QuadTo(1, 1, 3, 3);
    EXPECT_EQ(b.GetSegmentMasks(), skity::Path::SegmentMask::kQuad);
    a.Swap(b);
    EXPECT_EQ(a.GetSegmentMasks(), skity::Path::SegmentMask::kQuad);
    EXPECT_EQ(b.GetSegmentMasks(), skity::Path::SegmentMask::kLine);
  }

  // AddPath
  {
    skity::Path a;
    a.MoveTo(0, 0);
    a.LineTo(1, 1);
    EXPECT_EQ(a.GetSegmentMasks(), skity::Path::SegmentMask::kLine);
    skity::Path b;
    b.MoveTo(0, 0);
    b.QuadTo(1, 1, 3, 3);
    EXPECT_EQ(b.GetSegmentMasks(), skity::Path::SegmentMask::kQuad);
    a.AddPath(b);
    EXPECT_EQ(a.GetSegmentMasks(), skity::Path::SegmentMask::kLine |
                                       skity::Path::SegmentMask::kQuad);
  }

  // CopyWithMatrix
  {
    skity::Path a;
    a.MoveTo(0, 0);
    a.LineTo(1, 1);
    a.CubicTo(2, 2, 3, 3, 4, 4);
    EXPECT_EQ(a.GetSegmentMasks(), skity::Path::SegmentMask::kLine |
                                       skity::Path::SegmentMask::kCubic);
    skity::Path b = a.CopyWithMatrix(skity::Matrix::Translate(10, 10));
    EXPECT_EQ(b.GetSegmentMasks(), skity::Path::SegmentMask::kLine |
                                       skity::Path::SegmentMask::kCubic);
  }

  // CopyWithScale
  {
    skity::Path a;
    a.MoveTo(0, 0);
    a.LineTo(1, 1);
    a.CubicTo(2, 2, 3, 3, 4, 4);
    EXPECT_EQ(a.GetSegmentMasks(), skity::Path::SegmentMask::kLine |
                                       skity::Path::SegmentMask::kCubic);
    skity::Path b = a.CopyWithScale(3);
    EXPECT_EQ(b.GetSegmentMasks(), skity::Path::SegmentMask::kLine |
                                       skity::Path::SegmentMask::kCubic);
  }
}

TEST(Path, GetBounds_EmptyPath) {
  skity::Path path;
  auto bounds = path.GetBounds();
  EXPECT_TRUE(bounds.IsEmpty());
}

TEST(Path, GetBounds_SinglePoint) {
  skity::Path path;
  path.MoveTo(10, 20);
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 10.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 20.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 10.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 20.f);
}

TEST(Path, GetBounds_Line) {
  skity::Path path;
  path.MoveTo(0, 0);
  path.LineTo(100, 50);
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 50.f);
}

TEST(Path, GetBounds_Rectangle) {
  skity::Path path;
  path.AddRect(skity::Rect::MakeLTRB(10, 20, 100, 200));
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 10.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 20.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 200.f);
}

TEST(Path, GetBounds_QuadCurve) {
  skity::Path path;
  path.MoveTo(0, 0);
  path.QuadTo(50, 100, 100, 0);
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 100.f);
}

TEST(Path, GetBounds_CubicCurve) {
  skity::Path path;
  path.MoveTo(0, 0);
  path.CubicTo(25, 100, 75, 100, 100, 0);
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 100.f);
}

TEST(Path, GetBounds_NegativeCoordinates) {
  skity::Path path;
  path.MoveTo(-50, -100);
  path.LineTo(-20, -50);
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), -50.f);
  EXPECT_FLOAT_EQ(bounds.Top(), -100.f);
  EXPECT_FLOAT_EQ(bounds.Right(), -20.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), -50.f);
}

TEST(Path, GetBounds_MixedContours) {
  skity::Path path;
  path.MoveTo(0, 0);
  path.LineTo(50, 50);
  path.Close();
  path.MoveTo(100, 100);
  path.LineTo(200, 150);
  path.Close();
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 200.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 150.f);
}

TEST(Path, GetBounds_ComplexPath) {
  skity::Path path;
  path.MoveTo(10, 10);
  path.LineTo(50, 10);
  path.QuadTo(100, 100, 150, 50);
  path.CubicTo(200, 0, 250, 100, 300, 50);
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 10.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 300.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 100.f);
}

TEST(Path, GetBounds_Reset) {
  skity::Path path;
  path.MoveTo(0, 0);
  path.LineTo(100, 50);
  path.Reset();
  auto bounds = path.GetBounds();
  EXPECT_TRUE(bounds.IsEmpty());
}

TEST(Path, GetBounds_CopyWithScale) {
  skity::Path path;
  path.MoveTo(100, 100);
  path.LineTo(200, 200);
  path.MoveTo(100, 200);
  path.Close();
  auto scaled_path = path.CopyWithScale(3);
  auto bounds = scaled_path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 300.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 300.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 600.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 600.f);
}

TEST(Path, GetBounds_CopyWithTransform) {
  skity::Path path;
  path.MoveTo(100, 100);
  path.LineTo(200, 200);
  path.MoveTo(100, 200);
  path.Close();
  auto transform = skity::Matrix::Translate(50, 50);
  auto path1 = path.CopyWithMatrix(transform);
  auto bounds = path1.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 150.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 150.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 250.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 250.f);
}

TEST(Path, GetBounds_ModifyBounds) {
  skity::Path path;
  path.MoveTo(100, 100);
  path.LineTo(200, 200);
  path.MoveTo(100, 200);
  path.Close();
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 200.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 200.f);

  path.MoveTo(300, 300);
  path.LineTo(400, 400);
  path.Close();
  bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 400.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 400.f);
}

TEST(Path, GetBounds_SetLastPoint) {
  skity::Path path;
  path.MoveTo(100, 100);
  path.LineTo(200, 200);
  path.LineTo(100, 200);
  path.Close();
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 200.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 200.f);

  path.SetLastPt(0, 1);
  bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 1.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 200.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 200.f);
}

TEST(Path, GetBounds_Swap) {
  skity::Path a;
  a.MoveTo(0, 0);
  a.LineTo(100, 200);
  auto bounds_a = a.GetBounds();
  EXPECT_FLOAT_EQ(bounds_a.Left(), 0.f);
  EXPECT_FLOAT_EQ(bounds_a.Top(), 0.f);
  EXPECT_FLOAT_EQ(bounds_a.Right(), 100.f);
  EXPECT_FLOAT_EQ(bounds_a.Bottom(), 200.f);

  skity::Path b;
  b.MoveTo(-100, 50);
  b.QuadTo(50, 50, 300, 100);
  auto bounds_b = b.GetBounds();
  EXPECT_FLOAT_EQ(bounds_b.Left(), -100.f);
  EXPECT_FLOAT_EQ(bounds_b.Top(), 50.f);
  EXPECT_FLOAT_EQ(bounds_b.Right(), 300.f);
  EXPECT_FLOAT_EQ(bounds_b.Bottom(), 100.f);

  a.Swap(b);
  bounds_a = a.GetBounds();
  EXPECT_FLOAT_EQ(bounds_a.Left(), -100.f);
  EXPECT_FLOAT_EQ(bounds_a.Top(), 50.f);
  EXPECT_FLOAT_EQ(bounds_a.Right(), 300.f);
  EXPECT_FLOAT_EQ(bounds_a.Bottom(), 100.f);

  bounds_b = b.GetBounds();
  EXPECT_FLOAT_EQ(bounds_b.Left(), 0.f);
  EXPECT_FLOAT_EQ(bounds_b.Top(), 0.f);
  EXPECT_FLOAT_EQ(bounds_b.Right(), 100.f);
  EXPECT_FLOAT_EQ(bounds_b.Bottom(), 200.f);
}

TEST(Path, GetBounds_AddPath) {
  skity::Path a;
  a.MoveTo(-1, -1);
  a.LineTo(2, 2);

  auto bounds = a.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), -1.f);
  EXPECT_FLOAT_EQ(bounds.Top(), -1.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 2.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 2.f);

  skity::Path b;
  b.MoveTo(0, 0);
  b.QuadTo(1, 1, 3, 3);

  a.AddPath(b);
  bounds = a.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), -1.f);
  EXPECT_FLOAT_EQ(bounds.Top(), -1.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 3.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 3.f);
}

TEST(Path, GetBounds_MoveTos) {
  skity::Path path;
  // The first MoveTo will be ignored
  path.MoveTo(30, 30);
  path.MoveTo(100, 100);
  EXPECT_EQ(path.CountPoints(), 1);
  EXPECT_EQ(path.CountVerbs(), 1);

  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 100.f);

  path.Reset();
  path.MoveTo(10, 10);
  path.LineTo(20, 20);
  path.LineTo(10, 30);
  path.MoveTo(50, 60);
  path.MoveTo(60, 50);
  EXPECT_EQ(path.CountPoints(), 4);
  EXPECT_EQ(path.CountVerbs(), 4);
}

TEST(Path, GetBounds_IgnoreLastMoveTo) {
  skity::Path path;
  path.MoveTo(0, 0);
  path.LineTo(100, 50);
  auto bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 50.f);
  path.MoveTo(300, 300);
  bounds = path.GetBounds();
  EXPECT_FLOAT_EQ(bounds.Left(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Top(), 0.f);
  EXPECT_FLOAT_EQ(bounds.Right(), 100.f);
  EXPECT_FLOAT_EQ(bounds.Bottom(), 50.f);
  EXPECT_TRUE(path.IsFinite());

  path.MoveTo(std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::quiet_NaN());
  bounds = path.GetBounds();
  EXPECT_TRUE(bounds.IsEmpty());
  EXPECT_FALSE(path.IsFinite());
}

TEST(Path, GetType) {
  skity::Path path;
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);

  path.MoveTo(0, 0);
  path.LineTo(10, 10);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);

  path.Reset();
  path.AddRect(skity::Rect::MakeLTRB(10, 20, 100, 200));
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kRect);

  path.Reset();
  path.AddOval(skity::Rect::MakeLTRB(10, 20, 100, 200));
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kOval);

  path.Reset();
  path.AddRoundRect(skity::Rect::MakeLTRB(10, 20, 100, 200), 10, 10);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kSimpleRRect);

  path.SetLastPt(10, 5);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);
}

TEST(Path, Oval) {
  skity::Path path;
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);

  path.MoveTo(0, 0);
  path.LineTo(10, 10);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);

  path.Reset();
  path.AddRect(skity::Rect::MakeLTRB(10, 20, 100, 200));
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kRect);

  path.Reset();
  path.AddOval(skity::Rect::MakeLTRB(10, 20, 100, 200));

  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kOval);
  skity::Rect oval_rect = path.GetBounds();
  EXPECT_FLOAT_EQ(oval_rect.Left(), 10.f);
  EXPECT_FLOAT_EQ(oval_rect.Top(), 20.f);
  EXPECT_FLOAT_EQ(oval_rect.Right(), 100.f);
  EXPECT_FLOAT_EQ(oval_rect.Bottom(), 200.f);

  path.Reset();
  path.AddCircle(50, 50, 30);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kOval);
  skity::Rect circle_rect = path.GetBounds();
  EXPECT_FLOAT_EQ(circle_rect.Left(), 20.f);
  EXPECT_FLOAT_EQ(circle_rect.Top(), 20.f);
  EXPECT_FLOAT_EQ(circle_rect.Right(), 80.f);
  EXPECT_FLOAT_EQ(circle_rect.Bottom(), 80.f);

  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(10, 10);
  path.QuadTo(20, 20, 30, 30);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);
}

TEST(Path, IsSimpleRRect) {
  skity::Path path;

  // Empty path should not be RRect
  EXPECT_FALSE(path.IsSimpleRRect(nullptr));

  // Line path should not be RRect
  path.MoveTo(0, 0);
  path.LineTo(10, 10);
  EXPECT_FALSE(path.IsSimpleRRect(nullptr));

  // Rect path should not be RRect
  path.Reset();
  path.AddRect(skity::Rect::MakeLTRB(10, 20, 100, 200));
  EXPECT_FALSE(path.IsSimpleRRect(nullptr));

  // Oval path should not be RRect
  path.Reset();
  path.AddOval(skity::Rect::MakeLTRB(10, 20, 100, 200));
  EXPECT_FALSE(path.IsSimpleRRect(nullptr));

  // RoundRect path should be RRect
  path.Reset();
  path.AddRoundRect(skity::Rect::MakeLTRB(10, 20, 100, 200), 10, 10);
  skity::RRect rrect;
  EXPECT_TRUE(path.IsSimpleRRect(&rrect));
  EXPECT_FLOAT_EQ(rrect.GetRect().Left(), 10.f);
  EXPECT_FLOAT_EQ(rrect.GetRect().Top(), 20.f);
  EXPECT_FLOAT_EQ(rrect.GetRect().Right(), 100.f);
  EXPECT_FLOAT_EQ(rrect.GetRect().Bottom(), 200.f);
  EXPECT_FLOAT_EQ(rrect.GetSimpleRadii().x, 10.f);
  EXPECT_FLOAT_EQ(rrect.GetSimpleRadii().y, 10.f);

  // Complex path should not be RRect
  path.Reset();
  path.MoveTo(0, 0);
  path.LineTo(10, 10);
  path.QuadTo(20, 20, 30, 30);
  EXPECT_FALSE(path.IsSimpleRRect(nullptr));
}

TEST(Path, GetType_AfterModifications) {
  skity::Path path;

  // Start with rect
  path.AddRect(skity::Rect::MakeLTRB(10, 20, 100, 200));
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kRect);

  // Add more operations - should become general
  path.LineTo(150, 250);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);

  // Reset and create oval
  path.Reset();
  path.AddOval(skity::Rect::MakeLTRB(10, 20, 100, 200));
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kOval);

  // Reset and create rrect
  path.Reset();
  path.AddRoundRect(skity::Rect::MakeLTRB(10, 20, 100, 200), 10, 10);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kSimpleRRect);

  // After reset, should be general
  path.Reset();
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);

  path.MoveTo(5, 5);
  path.LineTo(10, 10);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);
  path.AddRoundRect(skity::Rect::MakeLTRB(10, 20, 100, 200), 10, 10);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);
}

TEST(Path, ResetClearsStateAndRetainsStorage) {
  skity::Path path;
  path.MoveTo(10, 20)
      .LineTo(30, 40)
      .QuadTo(50, 60, 70, 80)
      .ConicTo(90, 100, 110, 120, 0.5f)
      .CubicTo(130, 140, 150, 160, 170, 180)
      .Close()
      .MoveTo(200, 210);
  path.SetFillType(skity::Path::PathFillType::kEvenOdd);
  path.SetFirstDirection(skity::Path::Direction::kCW);
  EXPECT_FALSE(path.GetBounds().IsEmpty());
  path.SetConvexityType(skity::Path::ConvexityType::kConcave);
  const auto* points = path.Points();
  const auto* verbs = path.VerbsBegin();
  const auto* weights = path.ConicWeights();

  EXPECT_EQ(&path.Reset(), &path);
  skity::Path empty;
  EXPECT_TRUE(path.IsEmpty());
  EXPECT_EQ(path.CountPoints(), 0u);
  EXPECT_EQ(path.CountVerbs(), 0u);
  EXPECT_EQ(path.GetFillType(), empty.GetFillType());
  EXPECT_EQ(path.GetFirstDirection(), empty.GetFirstDirection());
  EXPECT_EQ(path.GetConvexityType(), empty.GetConvexityType());
  EXPECT_EQ(path.IsFinite(), empty.IsFinite());
  EXPECT_EQ(path.GetBounds(), empty.GetBounds());
  EXPECT_EQ(path.GetSegmentMasks(), 0u);
  EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);
  EXPECT_FALSE(path.Contains(20, 30));
  skity::Point pts[4];
  EXPECT_EQ(skity::Path::RawIter(path).Next(pts), skity::Path::Verb::kDone);

  // An implicit move must start at the origin, not the previous contour.
  path.ConicTo(2, 4, 6, 8, 0.75f).Close();
  skity::Path::RawIter iter(path);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kMove);
  EXPECT_EQ(pts[0], skity::Point(0, 0, 0, 1));
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kConic);
  EXPECT_FLOAT_EQ(iter.ConicWeight(), 0.75f);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kClose);
  EXPECT_EQ(iter.Next(pts), skity::Path::Verb::kDone);
  EXPECT_EQ(path.GetBounds(), skity::Rect::MakeLTRB(0, 0, 6, 8));
  EXPECT_EQ(path.GetSegmentMasks(), skity::Path::SegmentMask::kConic);
  EXPECT_EQ(path.Points(), points);
  EXPECT_EQ(path.VerbsBegin(), verbs);
  EXPECT_EQ(path.ConicWeights(), weights);
}

TEST(Path, ResetClearsNonFiniteAndSimpleShapeState) {
  skity::Path path;
  path.LineTo(1, 1).MoveTo(std::numeric_limits<float>::infinity(), 1);
  EXPECT_FALSE(path.IsFinite());
  path.Reset();
  EXPECT_TRUE(path.IsFinite());
  EXPECT_TRUE(path.GetBounds().IsEmpty());

  for (auto type : {skity::Path::IsAType::kRect, skity::Path::IsAType::kOval,
                    skity::Path::IsAType::kSimpleRRect}) {
    const auto bounds = skity::Rect::MakeLTRB(10, 20, 110, 220);
    if (type == skity::Path::IsAType::kRect) {
      path.AddRect(bounds);
    } else if (type == skity::Path::IsAType::kOval) {
      path.AddOval(bounds);
    } else {
      path.AddRoundRect(bounds, 5, 10);
    }
    ASSERT_EQ(path.GetIsAType(), type);
    EXPECT_EQ(path.GetBounds(), bounds);
    path.Reset();
    EXPECT_EQ(path.GetIsAType(), skity::Path::IsAType::kGeneral);
    EXPECT_FALSE(path.IsSimpleRRect(nullptr));
    EXPECT_FALSE(path.IsRect(nullptr));
    EXPECT_TRUE(path.GetBounds().IsEmpty());
  }
  path.AddRoundRect(skity::Rect::MakeLTRB(0, 0, 40, 60), 2, 3);
  skity::RRect rrect;
  ASSERT_TRUE(path.IsSimpleRRect(&rrect));
  EXPECT_EQ(rrect.GetSimpleRadii(), skity::Vec2(2, 3));
  EXPECT_TRUE(path.Contains(20, 30));
}

TEST(Path, PreserveSimpleShapeTransforms) {
  using P = skity::Path;
  const auto bounds = skity::Rect::MakeLTRB(10, 20, 100, 80);
  for (auto direction : {P::Direction::kCW, P::Direction::kCCW}) {
    std::array<P, 4> shapes;
    shapes[0].AddRect(bounds, direction, 2);
    shapes[1].AddOval(bounds, direction, 3);
    shapes[2].AddRoundRect(bounds, 5, 8, direction);
    shapes[3].AddCircle(40, 50, 20, direction);
    for (auto& src : shapes) {
      src.SetFillType(P::PathFillType::kEvenOdd);
      for (float sx : {2.f, -2.f}) {
        for (float sy : {3.f, -3.f}) {
          auto matrix = skity::Matrix::Scale(sx, sy);
          matrix.SetTranslateX(220);
          matrix.SetTranslateY(260);
          P appended;
          appended.AddPath(src, matrix);
          auto copied = src.CopyWithMatrix(matrix);
          for (const auto* dst : {&appended, &copied}) {
            EXPECT_EQ(dst->GetIsAType(), src.GetIsAType());
            EXPECT_EQ(dst->GetFirstDirection(), (sx < 0) == (sy < 0) ? direction
                                                : direction == P::Direction::kCW
                                                    ? P::Direction::kCCW
                                                    : P::Direction::kCW);
            ASSERT_EQ(dst->CountVerbs(), src.CountVerbs());
            ASSERT_EQ(dst->CountPoints(), src.CountPoints());
            for (size_t i = 0; i < src.CountVerbs(); ++i) {
              EXPECT_EQ(dst->GetVerb(i), src.GetVerb(i));
            }
            for (size_t i = 0; i < src.CountPoints(); ++i) {
              EXPECT_EQ(dst->GetPoint(i), matrix * src.GetPoint(i));
            }
            skity::RRect rrect;
            if (dst->IsSimpleRRect(&rrect)) {
              EXPECT_EQ(rrect.GetSimpleRadii(), skity::Vec2(10, 24));
            }
            size_t weight = 0;
            for (size_t i = 0; i < src.CountVerbs(); ++i) {
              if (src.GetVerb(i) == P::Verb::kConic) {
                EXPECT_EQ(dst->ConicWeights()[weight],
                          src.ConicWeights()[weight]);
                ++weight;
              }
            }
          }
          EXPECT_EQ(copied.GetFillType(), src.GetFillType());
          EXPECT_EQ(appended.GetFillType(), P::PathFillType::kWinding);
        }
      }
      auto scaled = src.CopyWithScale(-2);
      EXPECT_EQ(scaled.GetIsAType(), src.GetIsAType());
      EXPECT_EQ(scaled.GetFirstDirection(), direction);
      EXPECT_EQ(scaled.GetFillType(), src.GetFillType());
      scaled.SetLastPt(1, 2);
      EXPECT_EQ(scaled.GetIsAType(), P::IsAType::kGeneral);
    }
  }
}

TEST(Path, PreserveSimpleShapeRejectsUnsafeTransforms) {
  using P = skity::Path;
  std::vector<skity::Matrix> matrices{
      skity::Matrix::Scale(0, 2), skity::Matrix::RotateDeg(30),
      skity::Matrix::Skew(0.2f, 0), skity::Matrix::Scale(1e38f, 1),
      skity::Matrix::Translate(1e30f, 0)};
  // Each otherwise-identity z/perspective component must be rejected.
  for (auto rc : {skity::Vec2(0, 2), skity::Vec2(1, 2), skity::Vec2(2, 0),
                  skity::Vec2(2, 1), skity::Vec2(2, 2), skity::Vec2(2, 3),
                  skity::Vec2(3, 0), skity::Vec2(3, 1), skity::Vec2(3, 2),
                  skity::Vec2(3, 3)}) {
    skity::Matrix matrix;
    matrix.Set(rc.x, rc.y, 2);
    matrices.push_back(matrix);
  }
  for (float bad : {std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::quiet_NaN()}) {
    matrices.push_back(skity::Matrix::Scale(bad, 1));
    matrices.push_back(skity::Matrix::Translate(bad, 0));
  }
  std::array<P, 3> shapes;
  shapes[0].AddRect(skity::Rect::MakeWH(100, 80));
  shapes[1].AddOval(skity::Rect::MakeWH(100, 80));
  shapes[2].AddRoundRect(skity::Rect::MakeWH(100, 80), 5, 8);
  for (const auto& src : shapes) {
    for (const auto& matrix : matrices) {
      P dst;
      dst.AddPath(src, matrix);
      EXPECT_EQ(dst.GetIsAType(), P::IsAType::kGeneral);
      EXPECT_EQ(src.CopyWithMatrix(matrix).GetIsAType(), P::IsAType::kGeneral);
    }
    EXPECT_EQ(src.CopyWithScale(0).GetIsAType(), P::IsAType::kGeneral);
    P dst;
    dst.MoveTo(1, 2);
    dst.AddPath(src);
    EXPECT_EQ(dst.GetIsAType(), P::IsAType::kGeneral);
    dst = src;
    dst.AddPath(src);
    EXPECT_EQ(dst.GetIsAType(), P::IsAType::kGeneral);
    for (auto mode : {P::AddMode::kAppend, P::AddMode::kExtend}) {
      dst = src;
      dst.AddPath(dst, skity::Matrix(), mode);
      EXPECT_EQ(dst.GetIsAType(), P::IsAType::kGeneral);
      EXPECT_GT(dst.CountVerbs(), src.CountVerbs());
    }
    dst.Reset();
    dst.AddPath(src, P::AddMode::kExtend);
    EXPECT_EQ(dst.GetIsAType(), P::IsAType::kGeneral);
    dst = src.CopyWithScale(2);
    dst.LineTo(1, 2);
    EXPECT_EQ(dst.GetIsAType(), P::IsAType::kGeneral);
  }
  P collapsed;
  collapsed.AddRect(skity::Rect::MakeWH(0, 10));
  EXPECT_EQ(collapsed.CopyWithScale(2).GetIsAType(), P::IsAType::kGeneral);
  P rounded;
  rounded.AddRoundRect(skity::Rect::MakeWH(100, 80), 0.125f, 8);
  EXPECT_EQ(
      rounded.CopyWithMatrix(skity::Matrix::Translate(1e7f, 0)).GetIsAType(),
      P::IsAType::kGeneral);
}

TEST(Path, PreserveSimpleShapeRejectsInvalidSource) {
  using P = skity::Path;
  for (float invalid : {std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    P path;
    path.AddRect(skity::Rect::MakeLTRB(invalid, 0, 100, 80));
    EXPECT_EQ(path.CopyWithScale(2).GetIsAType(), P::IsAType::kGeneral);
  }
  P path;
  path.MoveTo(0, 0).Close();
  path.AddRect(skity::Rect::MakeWH(100, 80));
  EXPECT_EQ(path.CopyWithScale(2).GetIsAType(), P::IsAType::kGeneral);
}

TEST(Path, PreserveSimpleShapeAllStartsAndRepeatedTransforms) {
  using P = skity::Path;
  const auto bounds = skity::Rect::MakeLTRB(-20, -12, 36, 28);
  const auto rr = skity::RRect::MakeRectXY(bounds, 4, 6);
  for (auto direction : {P::Direction::kCW, P::Direction::kCCW}) {
    for (int start = 0; start < 8; ++start) {
      std::array<P, 3> shapes;
      shapes[0].AddRect(bounds, direction, start);
      shapes[1].AddOval(bounds, direction, start);
      shapes[2].AddRRect(rr, direction, start);
      for (const auto& src : shapes) {
        SCOPED_TRACE(::testing::Message()
                     << "start=" << start
                     << " direction=" << static_cast<int>(direction)
                     << " type=" << static_cast<int>(src.GetIsAType()));
        auto current = src;
        for (int step = 0; step < 4; ++step) {
          auto matrix = skity::Matrix::Scale(-2, 0.5f);
          matrix.SetTranslateX(128);
          matrix.SetTranslateY(32);
          auto copy = current.CopyWithMatrix(matrix);
          P append;
          append.AddPath(current, matrix);
          EXPECT_EQ(copy.GetIsAType(), src.GetIsAType());
          EXPECT_EQ(append.GetIsAType(), src.GetIsAType());
          EXPECT_EQ(copy.GetBounds(), append.GetBounds());
          EXPECT_EQ(copy.GetFirstDirection(), append.GetFirstDirection());
          for (size_t i = 0; i < src.CountPoints(); ++i) {
            EXPECT_EQ(copy.GetPoint(i), matrix * current.GetPoint(i));
            EXPECT_EQ(append.GetPoint(i), copy.GetPoint(i));
          }
          current = copy;
        }
        auto scaled = src.CopyWithScale(-2);
        EXPECT_EQ(scaled.GetIsAType(), src.GetIsAType());
        EXPECT_EQ(scaled.GetFirstDirection(), src.GetFirstDirection());
      }
    }
  }
}

TEST(Path, PreserveSimpleShapeRejectsEmptyContoursAndCenterOverflow) {
  using P = skity::Path;
  for (int shape = 0; shape < 3; ++shape) {
    P path;
    // Empty closed contours must prevent constructor metadata too.
    for (int i = 0; i < 32; ++i) path.MoveTo(i, i).Close();
    auto bounds = skity::Rect::MakeWH(100, 80);
    if (shape == 0) path.AddRect(bounds);
    if (shape == 1) path.AddOval(bounds);
    if (shape == 2) path.AddRoundRect(bounds, 4, 6);
    ASSERT_EQ(path.GetIsAType(), P::IsAType::kGeneral);
    EXPECT_EQ(path.CopyWithScale(2).GetIsAType(), P::IsAType::kGeneral);
    P append;
    append.AddPath(path);
    EXPECT_EQ(append.GetIsAType(), P::IsAType::kGeneral);
  }
  P oval;
  // Finite bounds but (left + right) overflows while constructing the center.
  oval.AddOval(skity::Rect::MakeLTRB(2e38f, 0, 3e38f, 80));
  EXPECT_EQ(oval.CopyWithScale(0.5f).GetIsAType(), P::IsAType::kGeneral);
  EXPECT_EQ(oval.CopyWithMatrix(skity::Matrix::Scale(0.5f, 1)).GetIsAType(),
            P::IsAType::kGeneral);
  P append;
  append.AddPath(oval);
  EXPECT_EQ(append.GetIsAType(), P::IsAType::kGeneral);
}

TEST(Path, PreserveSimpleShapeRejectsRoundedOvalCenter) {
  skity::Path oval;
  oval.AddOval(skity::Rect::MakeWH(1, 2));
  auto matrix = skity::Matrix::Scale(3, 3);
  matrix.SetTranslateX(0.1f);
  const auto left = matrix * skity::Point(0, 0, 0, 1);
  const auto right = matrix * skity::Point(1, 0, 0, 1);
  const auto center = matrix * skity::Point(0.5f, 0, 0, 1);
  ASSERT_NE(center.x, (left.x + right.x) * 0.5f);
  EXPECT_EQ(oval.CopyWithMatrix(matrix).GetIsAType(),
            skity::Path::IsAType::kGeneral);
  skity::Path append;
  append.AddPath(oval, matrix);
  EXPECT_EQ(append.GetIsAType(), skity::Path::IsAType::kGeneral);
}

// Opt in via the unified runner. Includes allocation and coordinate copying;
// source construction and result inspection are outside the timed interval.
TEST(Path, DISABLED_SimpleShapeTransformMicrobenchmark) {
  using P = skity::Path;
  std::array<P, 4> shapes;
  const auto bounds = skity::Rect::MakeLTRB(10, 20, 100, 80);
  shapes[0].AddRect(bounds);
  shapes[1].AddOval(bounds);
  shapes[2].AddRoundRect(bounds, 5, 8);
  shapes[3].MoveTo(0, 0).LineTo(10, 0).LineTo(5, 10).Close();
  const char* names[] = {"rect", "oval", "rrect", "general"};
  const char* operations[] = {"append", "matrix", "scale"};
  auto matrix = skity::Matrix::Scale(-2, 3);
  matrix.SetTranslateX(220);
  matrix.SetTranslateY(260);
  for (size_t shape = 0; shape < shapes.size(); ++shape) {
    for (int operation = 0; operation < 3; ++operation) {
      for (bool read_bounds : {false, true}) {
        std::array<double, 5> samples;
        for (auto& sample : samples) {
          std::vector<P> sources(20000, shapes[shape]);
          std::vector<P> results(sources.size());
          float width_sum = 0;
          const auto start = std::chrono::steady_clock::now();
          for (size_t i = 0; i < sources.size(); ++i) {
            if (operation == 0) results[i].AddPath(sources[i], matrix);
            if (operation == 1) results[i] = sources[i].CopyWithMatrix(matrix);
            if (operation == 2) results[i] = sources[i].CopyWithScale(-2);
            if (read_bounds) width_sum += results[i].GetBounds().Width();
          }
          const auto end = std::chrono::steady_clock::now();
          sample =
              std::chrono::duration<double, std::nano>(end - start).count() /
              sources.size();
          if (read_bounds) EXPECT_GT(width_sum, 0);
          for (const auto& result : results) {
            ASSERT_EQ(result.GetIsAType(), shapes[shape].GetIsAType());
            ASSERT_EQ(result.CountPoints(), shapes[shape].CountPoints());
          }
        }
        std::sort(samples.begin(), samples.end());
        RecordProperty(
            std::string(names[shape]) + "_" + operations[operation] +
                (read_bounds ? "_with_bounds_median_ns" : "_median_ns"),
            std::to_string(samples[2]));
      }
    }
  }
}

static skity::Path CanvasRoundRect() {
  skity::Path path;
  path.MoveTo(15, 20);
  path.LineTo(95, 20);
  path.ArcTo(100, 20, 100, 25, 5);
  path.LineTo(100, 55);
  path.ArcTo(100, 60, 95, 60, 5);
  path.LineTo(15, 60);
  path.ArcTo(10, 60, 10, 55, 5);
  path.LineTo(10, 25);
  path.ArcTo(10, 20, 15, 20, 5);
  return path;
}

// Opt in with GTEST_ALSO_RUN_DISABLED_TESTS=1 and the test-runner filter.
// Measures the read-only query and hit counting, excluding construction/draw.
TEST(Path, DISABLED_CanvasRRectRecognitionMicrobenchmark) {
  skity::Path triangle;
  triangle.MoveTo(0, 0).LineTo(10, 0).LineTo(5, 10).Close();
  auto rounded = CanvasRoundRect();
  rounded.Close();
  for (const auto& src : {triangle, rounded}) {
    std::array<double, 5> samples;
    size_t hits = 0;
    for (auto& sample : samples) {
      std::vector<skity::Path> paths(50000, src);
      hits = 0;
      skity::RRect rrect;
      const auto start = std::chrono::steady_clock::now();
      for (const auto& path : paths) {
        hits += skity::PathPriv::RecognizeCanvasRRect(path, &rrect);
      }
      const auto end = std::chrono::steady_clock::now();
      sample = std::chrono::duration<double, std::nano>(end - start).count() /
               paths.size();
    }
    std::sort(samples.begin(), samples.end());
    const std::string key = src.CountVerbs() == 4 ? "triangle" : "rrect";
    RecordProperty(key + "_median_ns", std::to_string(samples[2]));
    RecordProperty(key + "_hits_per_50000", std::to_string(hits));
    EXPECT_EQ(hits, src.CountVerbs() == 4 ? 0u : 50000u);
  }
}

TEST(Path, CanvasRoundRectCloseAndRecognitionKeepMetadataUnchanged) {
  using P = skity::Path;
  auto path = CanvasRoundRect();
  ASSERT_EQ(path.CountVerbs(), 13u);
  ASSERT_EQ(path.CountPoints(), 17u);
  auto original = path;
  auto untouched = skity::RRect::MakeRectXY(skity::Rect::MakeWH(12, 14), 2, 3);
  EXPECT_FALSE(skity::PathPriv::RecognizeCanvasRRect(path, &untouched));
  EXPECT_EQ(untouched.GetRect(), skity::Rect::MakeWH(12, 14));
  EXPECT_EQ(untouched.GetSimpleRadii(), skity::Vec2(2, 3));

  path.SetFillType(P::PathFillType::kEvenOdd);
  path.Close();
  path.Close();
  EXPECT_EQ(path.CountVerbs(), 14u);
  EXPECT_EQ(path.GetIsAType(), P::IsAType::kGeneral);
  EXPECT_FALSE(path.IsSimpleRRect(nullptr));
  auto direction = path.GetFirstDirection();
  skity::RRect rr;
  ASSERT_TRUE(skity::PathPriv::RecognizeCanvasRRect(path, &rr));
  EXPECT_EQ(rr.GetBounds(), skity::Rect::MakeLTRB(10, 20, 100, 60));
  EXPECT_EQ(rr.GetSimpleRadii(), skity::Vec2(5, 5));
  EXPECT_EQ(path.GetIsAType(), P::IsAType::kGeneral);
  EXPECT_EQ(path.GetFirstDirection(), direction);
  EXPECT_EQ(path.GetFillType(), P::PathFillType::kEvenOdd);
  EXPECT_EQ(path.GetLastMovePt(), original.GetPoint(0));
  for (size_t i = 0; i < original.CountPoints(); ++i)
    EXPECT_EQ(path.GetPoint(i), original.GetPoint(i));
  for (size_t i = 0; i < original.CountVerbs(); ++i)
    EXPECT_EQ(path.GetVerb(i), original.GetVerb(i));
  for (int i = 0; i < 4; ++i)
    EXPECT_EQ(path.ConicWeights()[i], original.ConicWeights()[i]);

  P appended;
  appended.AddPath(path, skity::Matrix::Scale(2, 3));
  for (const auto& copied :
       {appended, path.CopyWithMatrix(skity::Matrix::Scale(2, 3))}) {
    EXPECT_EQ(copied.GetIsAType(), P::IsAType::kGeneral);
    ASSERT_TRUE(skity::PathPriv::RecognizeCanvasRRect(copied, &rr));
    EXPECT_EQ(rr.GetSimpleRadii(), skity::Vec2(10, 15));
  }
  auto scaled = path.CopyWithScale(2);
  EXPECT_EQ(scaled.GetIsAType(), P::IsAType::kGeneral);
  EXPECT_TRUE(skity::PathPriv::RecognizeCanvasRRect(scaled, nullptr));
  // Reflected contours retain the same exact quarter arcs.
  EXPECT_TRUE(skity::PathPriv::RecognizeCanvasRRect(
      path.CopyWithMatrix(skity::Matrix::Scale(-2, 3)), nullptr));
}

TEST(Path, CanvasRoundRectRejectsNearMatches) {
  using P = skity::Path;
  auto original = CanvasRoundRect();
  std::array<skity::Point, 17> points;
  std::copy(original.Points(), original.Points() + points.size(),
            points.begin());
  std::array<float, 4> weights;
  std::copy(original.ConicWeights(), original.ConicWeights() + 4,
            weights.begin());
  auto rebuild = [&]() {
    P path;
    path.MoveTo(points[0]);
    for (size_t i = 0; i < 4; ++i) {
      path.LineTo(points[4 * i + 1]);
      path.LineTo(points[4 * i + 2]);
      path.ConicTo(points[4 * i + 3], points[4 * i + 4], weights[i]);
    }
    path.Close();
    return path;
  };
  ASSERT_TRUE(skity::PathPriv::RecognizeCanvasRRect(rebuild(), nullptr));
  for (auto& point : points) {
    for (int axis = 0; axis < 2; ++axis) {
      float saved = point[axis];
      for (float changed :
           {std::nextafter(saved, INFINITY), std::nextafter(saved, -INFINITY),
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) {
        point[axis] = changed;
        EXPECT_FALSE(skity::PathPriv::RecognizeCanvasRRect(rebuild(), nullptr));
      }
      point[axis] = saved;
    }
  }
  for (auto& weight : weights) {
    float saved = weight;
    for (float changed :
         {std::nextafter(saved, 1.f), std::nextafter(saved, 0.f), 0.5f,
          std::numeric_limits<float>::infinity(),
          std::numeric_limits<float>::quiet_NaN()}) {
      weight = changed;
      EXPECT_FALSE(skity::PathPriv::RecognizeCanvasRRect(rebuild(), nullptr));
    }
    weight = saved;
  }
  // Collapsed width and unequal corner radii must not be tagged.
  auto saved_points = points;
  for (auto& point : points) point.x = 10;
  EXPECT_FALSE(skity::PathPriv::RecognizeCanvasRRect(rebuild(), nullptr));
  points = saved_points;
  points[1].x = points[2].x = 94;
  points[4].y = 26;
  EXPECT_FALSE(skity::PathPriv::RecognizeCanvasRRect(rebuild(), nullptr));
  P multi;
  multi.MoveTo(1, 1).LineTo(2, 2).Close();
  multi.AddPath(original).Close();
  EXPECT_FALSE(skity::PathPriv::RecognizeCanvasRRect(multi, nullptr));
  P reverse;
  original.Close();
  P extended;
  extended.AddPath(original, P::AddMode::kExtend);
  EXPECT_EQ(extended.GetIsAType(), P::IsAType::kGeneral);
  // Draw-time recognition depends on the final contour, not append/extend
  // history.
  EXPECT_TRUE(skity::PathPriv::RecognizeCanvasRRect(extended, nullptr));
  auto nonplanar = CanvasRoundRect();
  skity::Matrix matrix;
  matrix.Set(2, 3, 1);
  nonplanar = nonplanar.CopyWithMatrix(matrix);
  nonplanar.Close();
  EXPECT_FALSE(skity::PathPriv::RecognizeCanvasRRect(nonplanar, nullptr));
  reverse.ReverseAddPath(original);
  EXPECT_TRUE(skity::PathPriv::RecognizeCanvasRRect(reverse, nullptr));
  // Canonical commands are recognized even after constructor metadata is lost.
  P direct;
  direct.AddRoundRect(skity::Rect::MakeLTRB(10, 20, 100, 60), 5, 5);
  auto last = direct.GetPoint(direct.CountPoints() - 1);
  direct.SetLastPt(last.x, last.y);
  direct.Close();
  EXPECT_TRUE(skity::PathPriv::RecognizeCanvasRRect(direct, nullptr));
}

TEST(Path, CanvasRoundRectRecognitionUsesCurrentContents) {
  using P = skity::Path;
  auto original = CanvasRoundRect();
  original.Close();
  ASSERT_TRUE(skity::PathPriv::RecognizeCanvasRRect(original, nullptr));
  for (int mutation = 0; mutation < 9; ++mutation) {
    P path = original;
    switch (mutation) {
      case 0:
        path.MoveTo(1, 2);
        break;
      case 1:
        path.LineTo(1, 2);
        break;
      case 2:
        path.QuadTo(1, 2, 3, 4);
        break;
      case 3:
        path.ConicTo(1, 2, 3, 4, 0.5f);
        break;
      case 4:
        path.CubicTo(1, 2, 3, 4, 5, 6);
        break;
      case 5:
        path.SetLastPt(1, 2);
        break;
      case 6:
        path.AddPath(original);
        break;
      case 7:
        path.Reset();
        break;
      case 8:
        path.ArcTo(100, 20, 100, 25, 5);
        break;
    }
    path.Close();
    EXPECT_EQ(path.GetIsAType(), P::IsAType::kGeneral);
    EXPECT_FALSE(skity::PathPriv::RecognizeCanvasRRect(path, nullptr));
  }
}

TEST(Path, CanvasRoundRectStartDirectionAndClosure) {
  using P = skity::Path;
  const auto rr = skity::RRect::MakeRectXY(skity::Rect::MakeWH(40, 30), 5, 7);
  for (auto direction : {P::Direction::kCW, P::Direction::kCCW}) {
    for (uint32_t start = 0; start < 8; ++start) {
      for (bool duplicates : {false, true}) {
        P canonical;
        canonical.AddRRect(rr, direction, start);
        P path;
        P::RawIter iter(canonical);
        skity::Point pts[4];
        for (auto verb = iter.Next(pts); verb != P::Verb::kDone;
             verb = iter.Next(pts)) {
          switch (verb) {
            case P::Verb::kMove:
              path.MoveTo(pts[0]);
              break;
            case P::Verb::kLine:
              path.LineTo(pts[1]);
              break;
            case P::Verb::kConic:
              if (duplicates) path.LineTo(pts[0]);
              path.ConicTo(pts[1], pts[2], iter.ConicWeight());
              break;
            default:
              break;  // Deliberately omit Close first.
          }
        }
        SCOPED_TRACE(::testing::Message()
                     << start << " " << int(direction) << " " << duplicates);
        EXPECT_EQ(path.GetIsAType(), P::IsAType::kGeneral);
        EXPECT_FALSE(skity::PathPriv::RecognizeCanvasRRect(path, nullptr));
        skity::RRect actual;
        ASSERT_TRUE(skity::PathPriv::RecognizeCanvasRRect(path, &actual, true));
        EXPECT_EQ(actual.GetRect(), rr.GetRect());
        EXPECT_EQ(actual.GetSimpleRadii(), rr.GetSimpleRadii());
        path.Close();
        EXPECT_TRUE(skity::PathPriv::RecognizeCanvasRRect(path, nullptr));
        // A nearly matching endpoint must not become a closing side.
        auto last = path.GetPoint(path.CountPoints() - 1);
        path.SetLastPt(last.x + 0.001f, last.y);
        EXPECT_FALSE(
            skity::PathPriv::RecognizeCanvasRRect(path, nullptr, true));
      }
    }
  }
}

TEST(Path, ShapeConstructorsRequireSingleContour) {
  using P = skity::Path;
  for (int shape = 0; shape < 3; ++shape) {
    for (int prefix = 0; prefix < 4; ++prefix) {
      P path;
      if (prefix > 0) path.MoveTo(-20, -30).MoveTo(-10, -15);
      if (prefix == 2) path.Close();
      if (prefix == 3) path.LineTo(-5, -5).Close();
      const auto bounds = skity::Rect::MakeWH(100, 80);
      if (shape == 0) path.AddRect(bounds);
      if (shape == 1) path.AddOval(bounds);
      if (shape == 2) path.AddRoundRect(bounds, 4, 6);
      SCOPED_TRACE(::testing::Message() << shape << " " << prefix);
      const bool single = prefix < 2;
      EXPECT_EQ(path.GetIsAType() != P::IsAType::kGeneral, single);
      EXPECT_EQ(path.CopyWithScale(-2).GetIsAType(), path.GetIsAType());
      if (single) {
        // A leading Move is replaced, not retained as an extra contour.
        EXPECT_EQ(path.GetBounds(), bounds);
        path.LineTo(123, 456);
        EXPECT_EQ(path.GetIsAType(), P::IsAType::kGeneral);
      } else {
        EXPECT_EQ(path.GetPoint(0), skity::Point(-10, -15, 0, 1));
        EXPECT_EQ(path.GetVerb(prefix == 2 ? 1 : 2), P::Verb::kClose);
      }
    }
  }
}
