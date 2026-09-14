/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/path_priv.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "src/geometry/math.hpp"
#include "src/geometry/point_priv.hpp"

namespace skity {

bool PathPriv::RecognizeCanvasRRect(const Path& path, RRect* rrect,
                                    bool allow_open) {
  using Verb = Path::Verb;
  // Four quarter conics, at most four sides and four redundant tangent lines.
  // Bound all subsequent work before inspecting commands or coordinates.
  if (path.CountVerbs() < 5 || path.CountVerbs() > 14 ||
      path.CountPoints() < 9 || path.CountPoints() > 17 ||
      path.GetVerb(0) != Verb::kMove)
    return false;
  const bool closed = path.GetVerb(path.CountVerbs() - 1) == Verb::kClose;
  if (!closed && !allow_open) return false;
  const size_t end = path.CountVerbs() - (closed ? 1 : 0);
  const auto* points = path.Points();
  std::array<const Point*, 4> arcs;
  size_t point = 1, count = 0;
  for (size_t i = 1; i < end; ++i) {
    if (path.GetVerb(i) == Verb::kConic) {
      if (count == arcs.size() || point + 1 >= path.CountPoints()) return false;
      arcs[count++] = points + point - 1;
      point += 2;
    } else if (path.GetVerb(i) == Verb::kLine) {
      ++point;
    } else {
      return false;  // No additional contours, closes or other curve types.
    }
  }
  if (count != 4 || point != path.CountPoints()) return false;
  for (size_t i = 0; i < 4; ++i) {
    if (path.ConicWeights()[i] != FloatRoot2Over2) return false;
  }
  float left = points[0].x, right = left;
  float top = points[0].y, bottom = top;
  for (size_t i = 0; i < path.CountPoints(); ++i) {
    if (!PointIsFinite(points[i]) || points[i].z != 0 || points[i].w != 1)
      return false;
    left = std::min(left, points[i].x);
    right = std::max(right, points[i].x);
    top = std::min(top, points[i].y);
    bottom = std::max(bottom, points[i].y);
  }
  const auto* first = arcs[0];
  const float rx = std::max(std::abs(first[1].x - first[0].x),
                            std::abs(first[1].x - first[2].x));
  const float ry = std::max(std::abs(first[1].y - first[0].y),
                            std::abs(first[1].y - first[2].y));
  if (!std::isfinite(right - left) || !std::isfinite(bottom - top) ||
      !(rx > 0 && ry > 0 && rx <= (right - left) * 0.5f &&
        ry <= (bottom - top) * 0.5f))
    return false;
  const float x0 = left + rx, x1 = right - rx;
  const float y0 = top + ry, y1 = bottom - ry;
  if (!(left < x0 && x1 < right && top < y0 && y1 < bottom)) return false;
  const std::array<Vec2, 4> corners = {Vec2{left, top}, Vec2{right, top},
                                       Vec2{right, bottom}, Vec2{left, bottom}};
  // Incoming/outgoing tangents in clockwise order, starting at the top left.
  const std::array<Vec2, 4> incoming = {Vec2{left, y0}, Vec2{x1, top},
                                        Vec2{right, y1}, Vec2{x0, bottom}};
  const std::array<Vec2, 4> outgoing = {Vec2{x0, top}, Vec2{right, y0},
                                        Vec2{x1, bottom}, Vec2{left, y1}};
  size_t corner = 0;
  while (corner < 4 && Vec2(first[1]) != corners[corner]) ++corner;
  if (corner == 4) return false;
  const bool clockwise = Vec2(first[0]) == incoming[corner];
  // No tolerance or curve fitting: verify all four exact quarter arcs in order.
  for (const auto* arc : arcs) {
    if (Vec2(arc[0]) != (clockwise ? incoming[corner] : outgoing[corner]) ||
        Vec2(arc[1]) != corners[corner] ||
        Vec2(arc[2]) != (clockwise ? outgoing[corner] : incoming[corner]))
      return false;
    corner = (corner + (clockwise ? 1 : 3)) % 4;
  }
  auto is_side = [&](const Point& a, const Point& b) {
    if (a == b) return true;  // Optional zero-length tangent lines.
    for (size_t i = 0; i < 4; ++i) {
      const auto& from = outgoing[i];
      const auto& to = incoming[(i + 1) % 4];
      if (Vec2(a) == (clockwise ? from : to) &&
          Vec2(b) == (clockwise ? to : from))
        return true;
    }
    return false;
  };
  point = 1;
  for (size_t i = 1; i < end; ++i) {
    if (path.GetVerb(i) == Verb::kLine) {
      if (!is_side(points[point - 1], points[point])) return false;
      ++point;
    } else {
      point += 2;
    }
  }
  // Close (or fill's implicit close) may supply the final straight side only.
  if (!is_side(points[point - 1], points[0])) return false;
  if (rrect) rrect->SetRectXY(Rect::MakeLTRB(left, top, right, bottom), rx, ry);
  return true;
}

void PathPriv::CreateDrawArcPath(Path* path, const Rect& oval, float startAngle,
                                 float sweepAngle, bool useCenter,
                                 bool isFillNoPathEffect) {
  if (isFillNoPathEffect && sweepAngle >= 360.f) {
    path->AddOval(oval);
    return;
  }

  if (useCenter) {
    path->MoveTo(oval.CenterX(), oval.CenterY());
  }

  bool forceMoveTo = !useCenter;
  while (sweepAngle <= -360.f) {
    path->ArcTo(oval, startAngle, -180.f, forceMoveTo);
    startAngle -= 180.f;
    path->ArcTo(oval, startAngle, -180.f, false);

    startAngle -= 180.f;
    forceMoveTo = false;
    sweepAngle += 360.f;
  }

  while (sweepAngle >= 360.f) {
    path->ArcTo(oval, startAngle, 180.f, forceMoveTo);
    startAngle += 180.f;
    path->ArcTo(oval, startAngle, 180.f, false);
    startAngle += 180.f;
    forceMoveTo = false;
    sweepAngle -= 360.f;
  }

  path->ArcTo(oval, startAngle, sweepAngle, forceMoveTo);
  if (useCenter) {
    path->Close();
  }
}

int32_t PathPriv::RectMakeDir(float dx, float dy) {
  return ((0 != dx) << 0) | ((dx > 0 || dy > 0) << 1);
}

}  // namespace skity
