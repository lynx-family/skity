// Copyright 2006 The Android Open Source Project.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_SW_SW_EDGE_HPP
#define SRC_RENDER_SW_SW_EDGE_HPP

#include <memory>
#include <skity/graphic/color.hpp>
#include <skity/graphic/path.hpp>
#include <vector>

#include "src/render/sw/sw_subpixel.hpp"

namespace skity {

struct SWEdge {
  SWEdge* prev;
  SWEdge* next;

  SWFixed x;
  SWFixed y;
  SWFixed dx;
  SWFixed dy;
  SWFixed upper_x;
  SWFixed upper_y;
  SWFixed lower_y;

  int8_t curve_count;
  uint8_t curve_shift;
  int8_t winding;

  static const int kDefaultAccuracy = 2;

  static inline SWFixed SnapY(SWFixed y) {
    const int accuracy = kDefaultAccuracy;
    // Keep 2 bits as decimal after rounding, so precision is 1/4.
    return ((unsigned)y + (SW_Fixed1 >> (accuracy + 1))) >>
           (16 - accuracy) << (16 - accuracy);
  }

  inline void GoY(SWFixed dst_y) {
    if (dst_y == y + SW_Fixed1) {
      x = x + dx;
      y = dst_y;
    } else if (y != dst_y) {
      x = upper_x + SWFixedMul(dx, y - upper_y);
      y = dst_y;
    }
  }

  // The value of y_shift here may be 0, 1 or 2, which means that y increases
  // by 1, 1/2 or 1/4 respectively.
  inline void GoY(SWFixed dst_y, int y_shift) {
    y = dst_y;
    x += dx >> y_shift;
  }

  bool SetLine(const Point& p0, const Point& p1);
  bool UpdateLine(SWFixed x0, SWFixed y0, SWFixed x1, SWFixed y1, SWFixed slop);
  bool CanBeIgnored(const Rect& scan_bounds, SWFixed y0, SWFixed y1) const;
};

struct SWQuadEdge : public SWEdge {
  SWFixed qx, qy;
  SWFixed qdx, qdy;
  SWFixed qddx, qddy;
  SWFixed q_first_y;
  SWFixed q_last_x, q_last_y;

  // snap y to integer points in the middle of the curve to accelerate AAA
  // path filling
  SWFixed snapped_x, snapped_y;

  bool SetQuad(const Point pts[3]);

  bool UpdateQuad();

  void KeepContinuous();
};

class SWEdgeBuilder {
 public:
  int BuildEdges(const Path& path, const Rect& scan_bounds);
  std::vector<std::unique_ptr<SWEdge>>& GetEdges() { return edges_; }

 private:
  void AddLine(const Point pts[], const Rect& scan_bounds);
  void AddQuad(const Point pts[], const Rect& scan_bounds);

  std::vector<std::unique_ptr<SWEdge>> edges_;
};

}  // namespace skity

#endif  // SRC_RENDER_SW_SW_EDGE_HPP
