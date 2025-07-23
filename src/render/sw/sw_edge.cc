// Copyright 2006 The Android Open Source Project.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_edge.hpp"

#include <array>
#include <cstdlib>
#include <utility>

#include "src/geometry/geometry.hpp"
#include "src/geometry/math.hpp"
#include "src/graphic/path_priv.hpp"

namespace skity {

bool SWEdge::SetLine(const Point& p0, const Point& p1) {
  // We must set X/Y using the same way (e.g., times 4, to FDot6, then to Fixed)
  // as Quads. Otherwise the order of the edge might be wrong due to
  // precision limit.
  const int accuracy = kDefaultAccuracy;
  const int multiplier = (1 << kDefaultAccuracy);
  SWFixed x0 = SWFDot6ToFixed(SkScalarToFDot6(p0.x * multiplier)) >> accuracy;
  SWFixed y0 =
      SnapY(SWFDot6ToFixed(SkScalarToFDot6(p0.y * multiplier)) >> accuracy);
  SWFixed x1 = SWFDot6ToFixed(SkScalarToFDot6(p1.x * multiplier)) >> accuracy;
  SWFixed y1 =
      SnapY(SWFDot6ToFixed(SkScalarToFDot6(p1.y * multiplier)) >> accuracy);
  winding = 1;
  SWFDot6 y0y1 = SkFixedToFDot6(y1 - y0);
  // horizontal line
  if (y0y1 == 0) {
    return false;
  }
  SWFDot6 x0x1 = SkFixedToFDot6(x1 - x0);
  SWFixed slope = SWFDot6Div(x0x1, y0y1);

  curve_count = 0;
  curve_shift = 0;
  return UpdateLine(x0, y0, x1, y1, slope);
}

bool SWEdge::UpdateLine(SWFixed x0, SWFixed y0, SWFixed x1, SWFixed y1,
                        SWFixed slope) {
  // We don't chop at y extrema for cubics so the y is not guaranteed to be
  // increasing for them. In that case, we have to swap x/y and negate the
  // winding.
  if (y0 > y1) {
    std::swap(x0, x1);
    std::swap(y0, y1);
    winding = -winding;
  }

  SWFDot6 x0x1 = SkFixedToFDot6(x1 - x0);
  SWFDot6 y0y1 = SkFixedToFDot6(y1 - y0);

  if (y0y1 == 0) {
    return false;
  }

  x = x0;
  y = y0;
  dx = slope;
  dy = (x0x1 == 0 || slope == 0) ? SW_MaxS32 : std::abs(SWFDot6Div(y0y1, x0x1));
  upper_x = x0;
  upper_y = y0;
  lower_y = y1;
  return true;
}

bool SWEdge::CanBeIgnored(const Rect& scan_bounds, SWFixed y0,
                          SWFixed y1) const {
  const int accuracy = kDefaultAccuracy;
  const int multiplier = (1 << kDefaultAccuracy);
  SWFixed start_y =
      SnapY(SWFDot6ToFixed(SkScalarToFDot6(scan_bounds.Top() * multiplier)) >>
            accuracy);
  SWFixed stop_y = SnapY(
      SWFDot6ToFixed(SkScalarToFDot6(scan_bounds.Bottom() * multiplier)) >>
      accuracy);

  if (y0 >= stop_y || y1 <= start_y) {
    return true;
  }

  return false;
}

/*  We store 1<<shift in a (signed) byte, so its maximum value is 1<<6 == 64.
    Note that this limits the number of lines we use to approximate a curve.
    If we need to increase this, we need to store fCurveCount in something
    larger than int8_t.
*/
#define MAX_COEFF_SHIFT 6

static inline SWFDot6 cheap_distance(SWFDot6 dx, SWFDot6 dy) {
  dx = std::abs(dx);
  dy = std::abs(dy);
  // return max + min/2
  return std::max(dx, dy) + (std::min(dx, dy) >> 1);
}

static inline int diff_to_shift(SWFDot6 dx, SWFDot6 dy, int shiftAA = 2) {
  // cheap calc of distance from center of p0-p2 to the center of the curve
  SWFDot6 dist = cheap_distance(dx, dy);

  // shift down dist (it is currently in dot6)
  // down by 3 should give us 1/8 pixel accuracy (assuming our dist is
  // accurate...) this is chosen by heuristic: make it as big as possible (to
  // minimize segments)
  // ... but small enough so that our curves still look smooth
  // When shift > 0, we're using AA and everything is scaled up so we can
  // lower the accuracy.
  dist = (dist + (1 << (2 + shiftAA))) >> (3 + shiftAA);

  // each subdivision (shift value) cuts this dist (error) by 1/4
  return (32 - CLZ(dist)) >> 1;
}

bool SWQuadEdge::SetQuad(const Point pts[3]) {
  SWFDot6 x0, y0, x1, y1, x2, y2;

  float scale = static_cast<float>(1 << (kDefaultAccuracy + 6));
  x0 = static_cast<int>(pts[0].x * scale);
  y0 = static_cast<int>(pts[0].y * scale);
  x1 = static_cast<int>(pts[1].x * scale);
  y1 = static_cast<int>(pts[1].y * scale);
  x2 = static_cast<int>(pts[2].x * scale);
  y2 = static_cast<int>(pts[2].y * scale);

  int w = 1;
  if (y0 > y2) {
    std::swap(x0, x2);
    std::swap(y0, y2);
    w = -1;
  }
  // The decimal part of y0 and y1 is 8 bits, and the decimal part of top and
  // bottom is 2 bits, so if the values ​​​​of top and bottom are less
  // than 1/4, nothing will be drawn.
  int top = SWFDot6Round(y0);
  int bottom = SWFDot6Round(y2);
  if (top == bottom) {
    return false;
  }

  // distance from center of p0-p2 to the center of the curve, it approximately
  // equal to half of the distance from center of p0-p2 to p1.
  SWFDot6 dx = (SWLeftShift(x1, 1) - x0 - x2) >> 2;  // (2 * x1 - x0 - x2) / 4
  SWFDot6 dy = (SWLeftShift(y1, 1) - y0 - y2) >> 2;  // (2 * y1 - y0 - y2) / 4
  int shift = diff_to_shift(dx, dy, kDefaultAccuracy);
  // need at least 1 subdivision for our bias trick
  if (shift == 0) {
    shift = 1;
  } else if (shift > MAX_COEFF_SHIFT) {
    shift = MAX_COEFF_SHIFT;
  }

  winding = w;
  curve_count = 1 << shift;

  /*
   *  We want to reformulate into polynomial form, to make it clear how we
   *  should forward-difference.
   *
   *  p0 (1 - t)^2 + p1 t(1 - t) + p2 t^2 ==> At^2 + Bt + C
   *
   *  A = p0 - 2p1 + p2
   *  B = 2(p1 - p0)
   *  C = p0
   *
   *  Our caller must have constrained our inputs (p0..p2) to all fit into
   *  16.16. However, as seen above, we sometimes compute values that can be
   *  larger (e.g. B = 2*(p1 - p0)). To guard against overflow, we will store
   *  A and B at 1/2 of their actual value, and just apply a 2x scale during
   *  application in updateQuadratic(). Hence we store (shift - 1) in
   *  fCurveShift.
   */
  curve_shift = shift - 1;

  SWFixed A = SWFDot6ToFixedDiv2(x0 - x1 - x1 + x2);  // 1/2 the real value
  SWFixed B = SWFDot6ToFixed(x1 - x0);                // 1/2 the real value

  qx = SWFDot6ToFixed(x0);
  // v = 2At + B
  // if t0 = 0, then v0 = B
  // if t1 = 1/count, then v1 = 2A / count + B
  // so qdx = (v0 + v1) / 2
  //        = A / count + B
  //        = A / 2^shift + B
  //        = B + A >> shift
  qdx = B + (A >> shift);  // biased by shift
  // v = 2At + B
  // if t0 = 0, then v0 = B
  // if t1 = 1/count, then v1 = 2A / count + B
  // so qddx = v1 - v0
  //         = 2A / count
  //         = 2A / 2^shift
  //         = A / 2^(shift-1)
  //         = A >> (shift - 1)
  qddx = A >> (shift - 1);  // biased by shift

  A = SWFDot6ToFixedDiv2(y0 - y1 - y1 + y2);  // 1/2 the real value
  B = SWFDot6ToFixed(y1 - y0);                // 1/2 the real value

  qy = SWFDot6ToFixed(y0);
  qdy = B + (A >> shift);   // biased by shift
  qddy = A >> (shift - 1);  // biased by shift

  q_last_x = SWFDot6ToFixed(x2);
  q_last_y = SWFDot6ToFixed(y2);

  qx >>= kDefaultAccuracy;
  qy >>= kDefaultAccuracy;
  qdx >>= kDefaultAccuracy;
  qdy >>= kDefaultAccuracy;
  qddx >>= kDefaultAccuracy;
  qddy >>= kDefaultAccuracy;
  q_last_x >>= kDefaultAccuracy;
  q_last_y >>= kDefaultAccuracy;
  qy = SnapY(qy);
  q_last_y = SnapY(q_last_y);
  q_first_y = qy;

  snapped_x = qx;
  snapped_y = qy;

  UpdateQuad();

  return true;
}

bool SWQuadEdge::UpdateQuad() {
  int success = 0;  // initialize to fail!
  int count = curve_count;
  SWFixed oldx = qx;
  SWFixed oldy = qy;
  SWFixed dx = qdx;
  SWFixed dy = qdy;
  SWFixed newx, newy, new_snapped_x, new_snapped_y;
  int shift = curve_shift;

  do {
    SWFixed slope;
    if (--count > 0) {
      // newx = oldx + v * t
      newx = oldx + (dx >> shift);
      newy = oldy + (dy >> shift);
      if (std::abs(dy >> shift) >=
          SW_Fixed1 * 2) {  // only snap when dy is large enough
        SWFDot6 diffY = SkFixedToFDot6(newy - snapped_y);
        slope = diffY ? SWFDot6Div(SkFixedToFDot6(newx - snapped_x), diffY)
                      : SW_MaxS32;
        // The precision of new_snapped_y is 1 pixel
        new_snapped_y = std::min<SWFixed>(q_last_y, SWFixedRoundToFixed(newy));
        new_snapped_x = newx - SWFixedMul(slope, newy - new_snapped_y);
      } else {
        // The precision of new_snapped_y is 1/4 pixel
        new_snapped_y = std::min(q_last_y, SnapY(newy));
        new_snapped_x = newx;
        SWFDot6 diffY = SkFixedToFDot6(new_snapped_y - snapped_y);
        slope = diffY ? SWFDot6Div(SkFixedToFDot6(newx - snapped_x), diffY)
                      : SW_MaxS32;
      }
      dx += qddx;
      dy += qddy;
    } else {  // last segment
      newx = q_last_x;
      newy = q_last_y;
      new_snapped_y = newy;
      new_snapped_x = newx;
      SWFDot6 diffY = (newy - snapped_y) >> 10;
      slope = diffY ? SWFDot6Div((newx - snapped_x) >> 10, diffY) : SW_MaxS32;
    }

    if (slope < SW_MaxS32) {
      success = this->UpdateLine(snapped_x, snapped_y, new_snapped_x,
                                 new_snapped_y, slope);
    }
    oldx = newx;
    oldy = newy;
  } while (count > 0 && !success);

  qx = newx;
  qy = newy;
  qdx = dx;
  qdy = dy;
  snapped_x = new_snapped_x;
  snapped_y = new_snapped_y;
  curve_count = count;
  return success;
}

void SWQuadEdge::KeepContinuous() {
  snapped_x = x;
  snapped_y = y;
}

int SWEdgeBuilder::BuildEdges(const Path& path, const Rect& scan_bounds) {
  PathEdgeIter iter(path);
  while (auto e = iter.next()) {
    switch (e.edge) {
      case skity::PathEdgeIter::Edge::kLine: {
        AddLine(e.points, scan_bounds);
        break;
      }
      case skity::PathEdgeIter::Edge::kQuad: {
        Point mono_x[5];
        int n = ChopQuadAtYExtrema(e.points, mono_x);
        for (int i = 0; i <= n; i++) {
          this->AddQuad(&mono_x[i * 2], scan_bounds);
        }
        break;
      }
      default:
        break;
    }
  }
  return edges_.size();
}

void SWEdgeBuilder::AddLine(const Point pts[], const Rect& scan_bounds) {
  auto edge = std::make_unique<SWEdge>();
  if (edge->SetLine(pts[0], pts[1]) &&
      !edge->CanBeIgnored(scan_bounds, edge->upper_y, edge->lower_y)) {
    edges_.push_back(std::move(edge));
  }
}

void SWEdgeBuilder::AddQuad(const Point pts[], const Rect& scan_bounds) {
  auto edge = std::make_unique<SWQuadEdge>();
  if (edge->SetQuad(pts) &&
      !edge->CanBeIgnored(scan_bounds, edge->q_first_y, edge->q_last_y)) {
    edges_.push_back(std::move(edge));
  }
}

}  // namespace skity
