// Copyright 2006 The Android Open Source Project

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <array>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <skity/graphic/path.hpp>
#include <sstream>
#include <tuple>

#include "src/geometry/conic.hpp"
#include "src/geometry/geometry.hpp"
#include "src/geometry/math.hpp"
#include "src/geometry/point_priv.hpp"
#include "src/graphic/path_priv.hpp"
#include "src/graphic/path_scanner.hpp"
#include "src/logging.hpp"

namespace skity {

static bool arc_is_long_point(Rect const& oval, float start_angle,
                              float sweep_angle, Point* pt) {
  if (0 == sweep_angle && (0 == start_angle || 360.f == start_angle)) {
    // Chrome uses this path to move into and out of ovals. If not
    // treated as a special case the moves can distort the oval's
    // bounding box (and break the circle special case).
    PointSet(*pt, oval.Right(), oval.CenterY());
    return true;
  } else if (0 == oval.Width() && 0 == oval.Height()) {
    // Chrome will sometimes create 0 radius round rects. Having degenerate
    // quad segments in the path prevents the path from being recognized as
    // a rect.
    // TODO(tangruiwen): optimizing the case where only one of width or height
    // is zero should also be considered. This case, however, doesn't seem to be
    // as common as the single point case.
    PointSet(*pt, oval.Right(), oval.Top());
    return true;
  }
  return false;
}

static std::tuple<Vec2, Vec2, RotationDirection> angles_to_unit_vectors(
    float startAngle, float sweepAngle) {
  float startRad = glm::radians(startAngle),
        stopRad = glm::radians(startAngle + sweepAngle);

  Vec2 startV;
  Vec2 stopV;
  RotationDirection dir;

  startV.y = FloatSinSnapToZero(startRad);
  startV.x = FloatCosSnapToZero(startRad);
  stopV.y = FloatSinSnapToZero(stopRad);
  stopV.x = FloatCosSnapToZero(stopRad);

  /*  If the sweep angle is nearly (but less than) 360, then due to precision
     loss in radians-conversion and/or sin/cos, we may end up with coincident
     vectors, which will fool SkBuildQuadArc into doing nothing (bad) instead
     of drawing a nearly complete circle (good).
     e.g. canvas.drawArc(0, 359.99, ...)
     -vs- canvas.drawArc(0, 359.9, ...)
     We try to detect this edge case, and tweak the stop vector
     */
  if (startV == stopV) {
    float sw = glm::abs(sweepAngle);
    if (sw < 360.f && sw > 359.f) {
      // make a guess at a tiny angle (in radians) to tweak by
      float deltaRad = FloatCopySign(Float1 / 512, sweepAngle);
      // not sure how much will be enough, so we use a loop
      do {
        stopRad -= deltaRad;
        stopV.y = FloatSinSnapToZero(stopRad);
        stopV.x = FloatCosSnapToZero(stopRad);
      } while (startV == stopV);
    }
  }

  dir = sweepAngle > 0 ? RotationDirection::kCW : RotationDirection::kCCW;

  return {startV, stopV, dir};
}

/**
 *  If this returns 0, then the caller should just line-to the singlePt, else it
 * should ignore singlePt and append the specified number of conics.
 */
static int build_arc_conics(const Rect& oval, const Vec2& start,
                            const Vec2& stop, RotationDirection dir,
                            Conic conics[Conic::kMaxConicsForArc],
                            Point* singlePt) {
  auto matrix = Matrix::Translate(oval.CenterX(), oval.CenterY()) *
                Matrix::Scale(oval.Width() * 0.5f, oval.Height() * 0.5f);

  int count = Conic::BuildUnitArc(start, stop, dir, &matrix, conics);
  if (0 == count) {
    *singlePt = matrix * Vec4{stop.x, stop.y, 0.f, 1.f};
  }
  return count;
}

template <size_t N>
class Path_PointIterator {
 public:
  using Array = std::array<Point, N>;

  Path_PointIterator(Path::Direction dir, size_t startIndex)
      : current_(startIndex % N),
        advance_(dir == Path::Direction::kCW ? 1 : N - 1) {}

  const Point& current() const {
    assert(current_ < N);
    return pts[current_];
  }

  const Point& next() {
    current_ = (current_ + advance_) % N;
    return this->current();
  }

 protected:
  Array pts;

 private:
  size_t current_;
  size_t advance_;
};

class Path_RectPointIterator : public Path_PointIterator<4> {
 public:
  Path_RectPointIterator(const Rect& rect, Path::Direction dir,
                         size_t startIndex)
      : Path_PointIterator<4>(dir, startIndex) {
    pts[0] = Point{rect.Left(), rect.Top(), 0, 1};
    pts[1] = Point{rect.Right(), rect.Top(), 0, 1};
    pts[2] = Point{rect.Right(), rect.Bottom(), 0, 1};
    pts[3] = Point{rect.Left(), rect.Bottom(), 0, 1};
  }
};

class Path_OvalPointIterator : public Path_PointIterator<4> {
 public:
  Path_OvalPointIterator(const Rect& oval, Path::Direction dir,
                         size_t startIndex)
      : Path_PointIterator<4>(dir, startIndex) {
    const float cx = oval.CenterX();
    const float cy = oval.CenterY();

    pts[0] = Point{cx, oval.Top(), 0, 1};
    pts[1] = Point{oval.Right(), cy, 0, 1};
    pts[2] = Point{cx, oval.Bottom(), 0, 1};
    pts[3] = Point{oval.Left(), cy, 0, 1};
  }
};

class Path_RRectPointIterator : public Path_PointIterator<8> {
 public:
  Path_RRectPointIterator(RRect const& rrect, Path::Direction dir,
                          uint32_t start_index)
      : Path_PointIterator(dir, start_index) {
    Rect const& bounds = rrect.GetBounds();
    float L = bounds.Left();
    float T = bounds.Top();
    float R = bounds.Right();
    float B = bounds.Bottom();

    pts[0] = Point{L + rrect.Radii(RRect::kUpperLeft).x, T, 0, 1.f};
    pts[1] = Point{R - rrect.Radii(RRect::kUpperRight).x, T, 0, 1.f};
    pts[2] = Point{R, T + rrect.Radii(RRect::kUpperRight).y, 0, 1.f};
    pts[3] = Point{R, B - rrect.Radii(RRect::kLowerRight).y, 0, 1.f};
    pts[4] = Point{R - rrect.Radii(RRect::kLowerRight).x, B, 0, 1.f};
    pts[5] = Point{L + rrect.Radii(RRect::kLowerLeft).x, B, 0, 1.f};
    pts[6] = Point{L, B - rrect.Radii(RRect::kLowerLeft).y, 0, 1.f};
    pts[7] = Point{L, T + rrect.Radii(RRect::kUpperLeft).y, 0, 1.f};
  }
};

class AutoDisableDirectionCheck {
 public:
  explicit AutoDisableDirectionCheck(Path* p)
      : path{p}, saved{p->GetFirstDirection()} {}
  ~AutoDisableDirectionCheck() { path->SetFirstDirection(saved); }

 private:
  Path* path;
  Path::Direction saved;
};

class AutoPathBoundsUpdate {
 public:
  AutoPathBoundsUpdate(Path* p, const Rect& r) : path{p}, rect{r} {
    rect.Sort();
    has_valid_bounds = path->IsFinite();
    empty = path->IsEmpty();
    if (has_valid_bounds && !empty) {
      JoinNoEmptyChecks(std::addressof(rect), path->GetBounds());
    }
    degenerate = is_degenerate(*path);
  }

  ~AutoPathBoundsUpdate() {
    if ((this->empty || has_valid_bounds) && rect.IsFinite()) {
      // TODO(tangruiwen) path->setBounds(rect);
    }
  }

 private:
  static void JoinNoEmptyChecks(Rect* dst, const Rect& src) {
    float s_left = src.Left();
    float s_top = src.Top();
    float s_right = src.Right();
    float s_bottom = src.Bottom();
    float d_left = dst->Left();
    float d_top = dst->Top();
    float d_right = dst->Right();
    float d_bottom = dst->Bottom();

    dst->SetLTRB(glm::min(s_left, d_left), glm::min(s_top, d_top),
                 glm::min(d_right, s_right), glm::min(d_bottom, s_bottom));
  }

  static bool is_degenerate(const Path& path) { return path.CountVerbs() <= 1; }

 private:
  Path* path;
  Rect rect;
  bool has_valid_bounds;
  bool degenerate;
  bool empty;
};

Path::Iter::Iter()
    : pts_{nullptr},
      verbs_{nullptr},
      verb_stop_{nullptr},
      conic_weights_{nullptr},
      force_close_(false),
      need_close_(false),
      close_line_(false),
      move_to_(),
      last_pt_(),
      segment_state_(SegmentState::kEmptyContour) {}

Path::Iter::Iter(const Path& path, bool forceClose) : Iter() {
  this->SetPath(path, forceClose);
}

void Path::Iter::SetPath(Path const& path, bool forceClose) {
  pts_ = path.points_.data();
  verbs_ = path.verbs_.data();
  verb_stop_ = path.verbs_.data() + path.CountVerbs();
  conic_weights_ = path.conic_weights_.data();
  if (conic_weights_) {
    conic_weights_ -= 1;
  }

  force_close_ = forceClose;
  need_close_ = false;
  segment_state_ = SegmentState::kEmptyContour;
}

Path::Verb Path::Iter::Next(Point pts[4]) {
  if (verbs_ == verb_stop_) {
    // Close the curve if requested and if there is some curve to close
    if (need_close_ && segment_state_ == SegmentState::kAfterPrimitive) {
      if (Verb::kLine == this->AutoClose(pts)) {
        return Verb::kLine;
      }
      need_close_ = false;
      return Verb::kClose;
    }
    return Verb::kDone;
  }

  Verb verb = *verbs_++;
  const Point* src_pts = pts_;
  Point* p_pts = pts;

  switch (verb) {
    case Verb::kMove:
      if (need_close_) {
        verbs_--;  // move back one verb
        verb = this->AutoClose(p_pts);
        if (verb == Verb::kClose) {
          need_close_ = false;
        }
        return verb;
      }
      if (verbs_ == verb_stop_) {
        return Verb::kDone;
      }
      move_to_ = *src_pts;
      p_pts[0] = *src_pts;
      src_pts += 1;
      segment_state_ = SegmentState::kAfterMove;
      last_pt_ = move_to_;
      need_close_ = force_close_;
      break;
    case Verb::kLine:
      p_pts[0] = this->ConsMoveTo();
      p_pts[1] = src_pts[0];
      last_pt_ = src_pts[0];
      close_line_ = false;
      src_pts += 1;
      break;
    case Verb::kConic:
      conic_weights_ += 1;
      // fall-through
      [[fallthrough]];
    case Verb::kQuad:
      p_pts[0] = this->ConsMoveTo();
      std::memcpy(std::addressof(p_pts[1]), src_pts, 2 * sizeof(Point));
      last_pt_ = src_pts[1];
      src_pts += 2;
      break;
    case Verb::kCubic:
      p_pts[0] = this->ConsMoveTo();
      std::memcpy(std::addressof(p_pts[1]), src_pts, 3 * sizeof(Point));
      last_pt_ = src_pts[2];
      src_pts += 3;
      break;
    case Verb::kClose:
      verb = this->AutoClose(p_pts);
      if (verb == Verb::kLine) {
        verbs_--;
      } else {
        need_close_ = false;
        segment_state_ = SegmentState::kEmptyContour;
      }
      last_pt_ = move_to_;
      break;
    case Verb::kDone:
      break;
  }
  pts_ = src_pts;
  return verb;
}

float Path::Iter::ConicWeight() const { return *conic_weights_; }

bool Path::Iter::IsClosedContour() const {
  if (verbs_ == nullptr || verbs_ == verb_stop_) {
    return false;
  }

  if (force_close_) {
    return true;
  }

  auto p_verbs = verbs_;
  auto p_stop = verb_stop_;

  if (*p_verbs == Verb::kMove) {
    p_verbs += 1;
  }

  while (p_verbs < p_stop) {
    auto v = *p_verbs++;
    if (v == Verb::kMove) {
      break;
    }
    if (v == Verb::kClose) {
      return true;
    }
  }

  return false;
}

Path::Verb Path::Iter::AutoClose(Point* pts) {
  if (last_pt_ != move_to_) {
    if (FloatIsNan(last_pt_.x) || FloatIsNan(last_pt_.y) ||
        FloatIsNan(move_to_.x) || FloatIsNan(move_to_.y)) {
      return Verb::kClose;
    }

    pts[0] = last_pt_;
    pts[1] = move_to_;
    last_pt_ = move_to_;
    close_line_ = true;
    return Verb::kLine;
  } else {
    pts[0] = move_to_;
    return Verb::kClose;
  }
}

const Point& Path::Iter::ConsMoveTo() {
  if (segment_state_ == SegmentState::kAfterMove) {
    segment_state_ = SegmentState::kAfterPrimitive;
    return move_to_;
  }

  return pts_[-1];
}

bool Path::Iter::IsCloseLine() const { return close_line_; }

Path::RawIter::RawIter()
    : pts_(nullptr),
      verbs_(nullptr),
      verb_stop_(nullptr),
      conic_weights_(nullptr) {}

void Path::RawIter::SetPath(const Path& path) {
  pts_ = path.points_.data();
  if (path.CountVerbs() > 0) {
    verbs_ = path.verbs_.data();
    verb_stop_ = path.verbs_.data() + path.CountVerbs();
  } else {
    verbs_ = verb_stop_ = nullptr;
  }

  conic_weights_ = path.conic_weights_.data();
  if (conic_weights_) {
    conic_weights_ -= 1;
  }

  if (!path.IsFinite()) {
    verb_stop_ = verbs_;
  }
}

Path::Verb Path::RawIter::Next(Point pts[4]) {
  if (verbs_ == verb_stop_) {
    return Verb::kDone;
  }

  auto verb = *verbs_++;
  auto src_pts = pts_;
  switch (verb) {
    case Verb::kMove:
      pts[0] = src_pts[0];
      src_pts += 1;
      break;

    case Verb::kLine:
      pts[0] = src_pts[-1];
      pts[1] = src_pts[0];
      src_pts += 1;
      break;

    case Verb::kConic:
      conic_weights_ += 1;
      // fall-through
      [[fallthrough]];
    case Verb::kQuad:
      pts[0] = src_pts[-1];
      pts[1] = src_pts[0];
      pts[2] = src_pts[1];
      src_pts += 2;
      break;
    case Verb::kCubic:
      pts[0] = src_pts[-1];
      pts[1] = src_pts[0];
      pts[2] = src_pts[1];
      pts[3] = src_pts[2];
      src_pts += 3;
      break;
    case Verb::kClose:
    case Verb::kDone:
    default:
      break;
  }
  pts_ = src_pts;
  return verb;
}

Path::Verb Path::RawIter::Peek() const {
  return verbs_ < verb_stop_ ? *verbs_ : Verb::kDone;
}

float Path::RawIter::ConicWeight() const { return *conic_weights_; }

static int _pts_in_verb(Path::Verb verb) {
  switch (verb) {
    case Path::Verb::kMove:
    case Path::Verb::kLine:
      return 1;
    case Path::Verb::kQuad:
    case Path::Verb::kConic:
      return 2;
    case Path::Verb::kCubic:
      return 3;
    case Path::Verb::kClose:
    case Path::Verb::kDone:
      return 0;
  }
  return 0;
}

Path& Path::MoveTo(float x, float y) {
  last_move_to_index_ = CountPoints();

  verbs_.emplace_back(Verb::kMove);
  points_.emplace_back(Point{x, y, 0, 1});

  return *this;
}

Path& Path::LineTo(float x, float y) {
  InjectMoveToIfNeed();

  verbs_.emplace_back(Verb::kLine);
  points_.emplace_back(Point{x, y, 0, 1});
  segment_masks_ |= SegmentMask::kLine;

  return *this;
}

Path& Path::QuadTo(float x1, float y1, float x2, float y2) {
  InjectMoveToIfNeed();

  verbs_.emplace_back(Verb::kQuad);
  points_.emplace_back(Point{x1, y1, 0, 1});
  points_.emplace_back(Point{x2, y2, 0, 1});
  segment_masks_ |= SegmentMask::kQuad;

  return *this;
}

Path& Path::ConicTo(float x1, float y1, float x2, float y2, float weight) {
  if (!(weight > 0)) {
    this->LineTo(x2, y2);
  } else if (glm::isinf(weight)) {
    this->LineTo(x1, y1);
    this->LineTo(x2, y2);
  } else if (weight == Float1) {
    this->QuadTo(x1, y1, x2, y2);
  } else {
    InjectMoveToIfNeed();

    verbs_.emplace_back(Verb::kConic);
    conic_weights_.emplace_back(weight);
    points_.emplace_back(Point{x1, y1, 0, 1});
    points_.emplace_back(Point{x2, y2, 0, 1});
    segment_masks_ |= SegmentMask::kConic;
  }
  return *this;
}

Path& Path::CubicTo(float x1, float y1, float x2, float y2, float x3,
                    float y3) {
  InjectMoveToIfNeed();

  verbs_.emplace_back(Verb::kCubic);

  points_.emplace_back(Point{x1, y1, 0, 1});
  points_.emplace_back(Point{x2, y2, 0, 1});
  points_.emplace_back(Point{x3, y3, 0, 1});
  segment_masks_ |= SegmentMask::kCubic;

  return *this;
}

Path& Path::ArcTo(float x1, float y1, float x2, float y2, float radius) {
  if (radius == 0) {
    return LineTo(x1, y1);
  }

  Point start;
  GetLastPt(std::addressof(start));
  // need to know prev point so we can construct tangent vectors
  glm::dvec4 befored, afterd;

  befored = glm::normalize(glm::dvec4{x1 - start.x, y1 - start.y, 0, 0});
  afterd = glm::normalize(glm::dvec4{x2 - x1, y2 - y1, 0, 0});

  double cosh = glm::dot(befored, afterd);
  double sinh = befored.x * afterd.y - befored.y * afterd.x;

  if (!PointIsFinite(befored) || !PointIsFinite(afterd) ||
      FloatNearlyZero(static_cast<float>(sinh))) {
    return LineTo(x1, y1);
  }

  Vec4 before =
      Vec4{static_cast<float>(befored.x), static_cast<float>(befored.y),
           static_cast<float>(befored.z), static_cast<float>(befored.w)};
  Vec4 after = Vec4{static_cast<float>(afterd.x), static_cast<float>(afterd.y),
                    static_cast<float>(afterd.z), static_cast<float>(afterd.w)};

  float dist = glm::abs(static_cast<float>(radius * (1 - cosh) / sinh));
  float xx = x1 - dist * before.x;
  float yy = y1 - dist * before.y;
  PointSetLength<false>(after, after.x, after.y, dist);

  LineTo(xx, yy);
  float weight = glm::sqrt(static_cast<float>(FloatHalf + cosh * 0.5));

  return ConicTo(x1, y1, x1 + after.x, y1 + after.y, weight);
}

Path& Path::ArcTo(Rect const& oval, float startAngle, float sweepAngle,
                  bool forceMove) {
  if (oval.Width() < 0 || oval.Height() < 0) {
    return *this;
  }

  if (CountVerbs() == 0) {
    forceMove = true;
  }

  Point long_pt;
  if (arc_is_long_point(oval, startAngle, sweepAngle, &long_pt)) {
    return forceMove ? this->MoveTo(long_pt) : this->LineTo(long_pt);
  }

  Vec2 startV, stopV;
  RotationDirection dir;
  std::tie(startV, stopV, dir) = angles_to_unit_vectors(startAngle, sweepAngle);

  Point singlePt;

  // Adds a move-to to 'pt' if forceMoveTo is true. Otherwise a LineTo unless
  // we're sufficiently close to 'pt' currently. This prevents spurious LineTos
  // when adding a series of contiguous arcs from the same oval.
  auto addPt = [&forceMove, this](const Point& pt) {
    Point lastPt;
    if (forceMove) {
      this->MoveTo(pt);
    } else if (!this->GetLastPt(&lastPt) || !FloatNearlyZero(lastPt.x - pt.x) ||
               !FloatNearlyZero(lastPt.y - pt.y)) {
      this->LineTo(pt);
    }
  };

  // At this point, we know that the arc is not a lone point, but startV ==
  // stopV indicates that the sweepAngle is too small such that
  // angles_to_unit_vectors cannot handle it.
  if (startV == stopV) {
    float endAngle = glm::radians(startAngle + sweepAngle);
    float radiusX = oval.Width() / 2;
    float radiusY = oval.Height() / 2;
    // We do not use SkScalar[Sin|Cos]SnapToZero here. When sin(startAngle) is 0
    // and sweepAngle is very small and radius is huge, the expected behavior
    // here is to draw a line. But calling SkScalarSinSnapToZero will make
    // sin(endAngle) be 0 which will then draw a dot.
    PointSet(singlePt, oval.CenterX() + radiusX * glm::cos(endAngle),
             oval.CenterY() + radiusY * glm::sin(endAngle));
    addPt(singlePt);
    return *this;
  }

  Conic conics[Conic::kMaxConicsForArc];
  int count = build_arc_conics(oval, startV, stopV, dir, conics, &singlePt);
  if (count) {
    const Point& pt = conics[0].pts[0];
    addPt(pt);
    for (int i = 0; i < count; ++i) {
      this->ConicTo(conics[i].pts[1], conics[i].pts[2], conics[i].w);
    }
  } else {
    addPt(singlePt);
  }
  return *this;
}

Path& Path::ArcTo(float rx, float ry, float xAxisRotate, ArcSize largeArc,
                  Direction sweep, float x, float y) {
  // convert degree to angle
  xAxisRotate = glm::radians(xAxisRotate);
  this->InjectMoveToIfNeed();
  std::array<Point, 2> src_pts{};
  this->GetLastPt(src_pts.data());
  // If rx = 0 or ry = 0 then this arc is treated as a straight line segment
  // joining the endpoints.
  // http://www.w3.org/TR/SVG/implnote.html#ArcOutOfRangeParameters
  if (!rx || !ry) {
    return this->LineTo(x, y);
  }
  // If the current point and target point for the arc are identical, it should
  // be treated as a zero length path. This ensures continuity in animations.
  src_pts[1].x = x;
  src_pts[1].y = y;
  src_pts[1].z = 0;
  src_pts[1].w = 1;

  if (src_pts[0] == src_pts[1]) {
    return this->LineTo(x, y);
  }

  rx = std::abs(rx);
  ry = std::abs(ry);
  Vector mid_point_distance = (src_pts[0] - src_pts[1]) * 0.5f;

  Matrix point_transform = Matrix::RotateRad(-xAxisRotate);

  Point transformed_mid_point = point_transform * mid_point_distance;
  float square_rx = rx * rx;
  float square_ry = ry * ry;
  float square_x = transformed_mid_point.x * transformed_mid_point.x;
  float square_y = transformed_mid_point.y * transformed_mid_point.y;

  // Check if the radii are big enough to draw the arc, scale radii if not.
  //  http://www.w3.org/TR/SVG/implnote.html#ArcCorrectionOutOfRangeRadii
  float radii_scale = square_x / square_rx + square_y / square_ry;
  if (radii_scale > 1.f) {
    radii_scale = std::sqrt(radii_scale);
    rx *= radii_scale;
    ry *= radii_scale;
  }

  point_transform = Matrix::Scale(1.f / rx, 1.f / ry);

  point_transform = point_transform * Matrix::RotateRad(-xAxisRotate);

  std::array<Point, 2> unit_pts{};
  unit_pts[0] = point_transform * src_pts[0];
  unit_pts[1] = point_transform * src_pts[1];

  Vector delta = unit_pts[1] - unit_pts[0];
  float d = delta.x * delta.x + delta.y * delta.y;
  float scale_factor_squared = std::max(1.f / d - 0.25f, 0.f);
  float scale_factor = std::sqrt(scale_factor_squared);

  if ((sweep == Direction::kCCW) != static_cast<bool>(largeArc)) {
    scale_factor = -scale_factor;
  }

  PointScale(delta, scale_factor, &delta);
  Point center_point = (unit_pts[0] + unit_pts[1]) * 0.5f;
  center_point.x -= delta.y;
  center_point.y += delta.x;
  unit_pts[0] -= center_point;
  unit_pts[1] -= center_point;
  float theta1 = std::atan2(unit_pts[0].y, unit_pts[0].x);
  float theta2 = std::atan2(unit_pts[1].y, unit_pts[1].x);
  float theta_arc = theta2 - theta1;
  if (theta_arc < 0 && (sweep == Direction::kCW)) {
    // sweep flipped from the original implementation
    theta_arc += glm::pi<float>() * 2.f;
  } else if (theta_arc > 0 && (sweep != Direction::kCW)) {
    theta_arc -= glm::pi<float>() * 2.f;
  }

  // Very tiny angles cause our subsequent math to go wonky (skbug.com/9272)
  // so we do a quick check here. The precise tolerance amount is just made up.
  // PI/million happens to fix the bug in 9272, but a larger value is probably
  // ok too.
  if (std::abs(theta_arc) < (glm::pi<float>() / (1000.f * 1000.f))) {
    return this->LineTo(x, y);
  }

  point_transform = Matrix::RotateRad(xAxisRotate);
  point_transform = point_transform * Matrix::Scale(rx, ry);

  // the arc may be slightly bigger than 1/4 circle, so allow up to 1/3rd
  int segments =
      std::ceil(std::abs(theta_arc / (2.f * glm::pi<float>() / 3.f)));
  float theta_width = theta_arc / segments;
  float t = std::tan(theta_width * 0.5f);

  if (!FloatIsFinite(t)) {
    return *this;
  }

  float start_theta = theta1;
  float w = std::sqrt(FloatHalf + std::cos(theta_width) * FloatHalf);
  auto float_is_integer = [](float scalar) -> bool {
    return scalar == std::floor(scalar);
  };

  bool expect_integers =
      FloatNearlyZero(glm::pi<float>() / 2.f - std::abs(theta_width)) &&
      float_is_integer(rx) && float_is_integer(ry) && float_is_integer(x) &&
      float_is_integer(y);

  for (int i = 0; i < segments; i++) {
    float end_theta = start_theta + theta_width;
    float sin_end_theta = FloatSinSnapToZero(end_theta);
    float cos_end_theta = FloatCosSnapToZero(end_theta);

    PointSet(unit_pts[1], cos_end_theta, sin_end_theta);
    unit_pts[1] += center_point;
    unit_pts[1].w = 1;  // FIXME

    unit_pts[0] = unit_pts[1];
    unit_pts[0].x += t * sin_end_theta;
    unit_pts[0].y += -t * cos_end_theta;

    std::array<Point, 2> mapped;
    mapped[0] = point_transform * unit_pts[0];
    mapped[1] = point_transform * unit_pts[1];

    if (expect_integers) {
      for (auto& point : mapped) {
        point.x = std::round(point.x);
        point.y = std::round(point.y);
      }
    }

    this->ConicTo(mapped[0], mapped[1], w);
    start_theta = end_theta;
  }

  return *this;
}

Path& Path::AddRect(Rect const& rect, Direction dir, uint32_t start) {
  this->SetFirstDirection(this->HasOnlyMoveTos() ? dir : Direction::kUnknown);

  AutoDisableDirectionCheck addc{this};
  AutoPathBoundsUpdate adbu(this, rect);

  Path_RectPointIterator iter{rect, dir, start};

  this->MoveTo(iter.current());
  this->LineTo(iter.next());
  this->LineTo(iter.next());
  this->LineTo(iter.next());
  this->Close();

  return *this;
}

Path& Path::Close() {
  size_t count = CountVerbs();
  if (count > 0) {
    switch (verbs_.back()) {
      case Verb::kLine:
      case Verb::kQuad:
      case Verb::kConic:
      case Verb::kCubic:
      case Verb::kMove:
        verbs_.emplace_back(Verb::kClose);
        break;
      case Verb::kClose:
        break;
      default:
        break;
    }
  }

  last_move_to_index_ ^=
      ~last_move_to_index_ >> (8 * sizeof(last_move_to_index_) - 1);

  return *this;
}

Path& Path::AddRoundRect(Rect const& rect, float rx, float ry, Direction dir) {
  if (rx < 0 || ry < 0) {
    return *this;
  }

  RRect rrect;
  rrect.SetRectXY(rect, rx, ry);

  return this->AddRRect(rrect, dir);
}

Path& Path::AddRoundRect(Rect const& rect, const float radii[], Direction dir) {
  RRect rrect;
  const Vec2 radii_[4] = {Vec2{radii[0], radii[1]}, Vec2{radii[2], radii[3]},
                          Vec2{radii[4], radii[5]}, Vec2{radii[6], radii[7]}};
  rrect.SetRectRadii(rect, radii_);

  return this->AddRRect(rrect, dir);
}

Path& Path::AddRRect(RRect const& rrect, Direction dir) {
  // legacy start indices: 6 (CW) and 7(CCW)
  return this->AddRRect(rrect, dir, dir == Direction::kCW ? 6 : 7);
}

Path& Path::AddRRect(RRect const& rrect, Direction dir, uint32_t start_index) {
  Rect const& bounds = rrect.GetBounds();

  if (rrect.IsRect() || rrect.IsEmpty()) {
    // degenerate(rect) => radii points are collapsing
    this->AddRect(bounds, dir, (start_index + 1) / 2);
  } else if (rrect.IsOval()) {
    // degenerate(oval) => line points are collapsing
    this->AddOval(bounds, dir, start_index / 2);
  } else {
    this->SetFirstDirection(this->HasOnlyMoveTos() ? dir : Direction::kUnknown);

    AutoPathBoundsUpdate apbu{this, bounds};
    AutoDisableDirectionCheck addc{this};

    // we start with a conic on odd indices when moving CW vs.
    // even indices when moving CCW
    bool starts_with_conic = ((start_index & 1) == (dir == Direction::kCW));
    float weight = FloatRoot2Over2;

    Path_RRectPointIterator rrect_iter{rrect, dir, start_index};
    // Corner iterator indices follow the collapsed radii model,
    // adjusted such that the start pt is "behind" the radii start pt.
    uint32_t rect_start_index =
        start_index / 2 + (dir == Direction::kCW ? 0 : 1);
    Path_RectPointIterator rect_iter{bounds, dir, rect_start_index};

    this->MoveTo(rrect_iter.current());
    if (starts_with_conic) {
      for (uint32_t i = 0; i < 3; i++) {
        this->ConicTo(rect_iter.next(), rrect_iter.next(), weight);
        this->LineTo(rrect_iter.next());
      }
      this->ConicTo(rect_iter.next(), rrect_iter.next(), weight);
      // final LineTo handled by close().
    } else {
      for (uint32_t i = 0; i < 4; i++) {
        this->LineTo(rrect_iter.next());
        this->ConicTo(rect_iter.next(), rrect_iter.next(), weight);
      }
    }

    this->Close();
  }

  return *this;
}

Path& Path::Reset() {
  *this = Path();
  return *this;
}

Path& Path::ReverseAddPath(const Path& src) {
  auto verbs_begin = src.verbs_.data();
  auto verbs = verbs_begin + src.verbs_.size();
  auto pts = src.points_.data() + src.CountPoints();
  auto conic_weights = src.conic_weights_.data() + src.conic_weights_.size();

  bool need_move = true;
  bool need_close = false;
  while (verbs > verbs_begin) {
    auto v = *--verbs;
    int n = _pts_in_verb(v);

    if (need_move) {
      --pts;
      MoveTo(pts->x, pts->y);
      need_move = false;
    }

    pts -= n;

    switch (v) {
      case Verb::kMove:
        if (need_close) {
          Close();
          need_close = false;
        }
        need_move = true;
        pts += 1;
        break;
      case Verb::kLine:
        LineTo(pts->x, pts->y);
        break;
      case Verb::kQuad:
        QuadTo(pts[1].x, pts[1].y, pts[0].x, pts[0].y);
        break;
      case Verb::kConic:
        ConicTo(pts[1], pts[0], *--conic_weights);
        break;
      case Verb::kCubic:
        CubicTo(pts[2].x, pts[2].y, pts[1].x, pts[1].y, pts[0].x, pts[0].y);
        break;
      case Verb::kClose:
        need_close = true;
        break;
      default:
        break;
    }
  }

  return *this;
}

Path& Path::AddCircle(float x, float y, float radius, Direction dir) {
  if (radius > 0) {
    AddOval(Rect::MakeLTRB(x - radius, y - radius, x + radius, y + radius),
            dir);
  }

  return *this;
}

Path& Path::AddOval(const Rect& oval, Direction dir) {
  return AddOval(oval, dir, 1);
}

Path& Path::AddOval(const Rect& oval, Direction dir, uint32_t start) {
  bool is_oval = HasOnlyMoveTos();
  if (is_oval) {
    first_direction_ = dir;
  } else {
    first_direction_ = Direction::kUnknown;
  }

  AutoDisableDirectionCheck addc{this};
  AutoPathBoundsUpdate apbu{this, oval};

  Path_OvalPointIterator oval_iter{oval, dir, start};
  Path_RectPointIterator rect_iter{oval, dir,
                                   start + (dir == Direction::kCW ? 0 : 1)};

  float weight = FloatRoot2Over2;

  MoveTo(oval_iter.current());
  for (uint32_t i = 0; i < 4; i++) {
    ConicTo(rect_iter.next(), oval_iter.next(), weight);
  }
  Close();
  return *this;
}

Path& Path::ReversePathTo(const Path& src) {
  if (src.verbs_.empty()) {
    return *this;
  }

  auto verbs = src.verbs_.data() + src.verbs_.size();
  auto verbs_begin = src.verbs_.data();
  const Point* pts = src.points_.data() + src.points_.size() - 1;
  const float* conic_weights =
      src.conic_weights_.data() + src.conic_weights_.size();

  while (verbs > verbs_begin) {
    auto v = *--verbs;
    pts -= _pts_in_verb(v);
    switch (v) {
      case Verb::kMove:
        // if the path has multiple contours, stop after reversing the last
        return *this;
      case Verb::kLine:
        LineTo(pts[0].x, pts[0].y);
        break;
      case Verb::kQuad:
        QuadTo(pts[1].x, pts[1].y, pts[0].x, pts[0].y);
        break;
      case Verb::kConic:
        ConicTo(pts[1].x, pts[1].y, pts[0].x, pts[0].y, *--conic_weights);
        break;
      case Verb::kCubic:
        CubicTo(pts[2].x, pts[2].y, pts[1].x, pts[1].y, pts[0].x, pts[0].y);
        break;
      case Verb::kClose:
      case Verb::kDone:
      default:
        break;
    }
  }

  return *this;
}

bool Path::GetLastPt(Point* lastPt) const {
  size_t count = CountPoints();
  if (count > 0) {
    if (lastPt) {
      *lastPt = points_.back();
    }
    return true;
  }

  if (lastPt) {
    *lastPt = {0, 0, 0, 1};
  }
  return false;
}

Point Path::GetPoint(int index) const {
  if (index < static_cast<int32_t>(CountPoints())) {
    return points_[index];
  }
  return Point{0, 0, 0, 1};
}

Path::Verb Path::GetVerb(int index) const {
  if (index < static_cast<int32_t>(CountVerbs())) {
    return verbs_[index];
  }
  return Path::Verb::kDone;
}

Point Path::GetLastMovePt() const {
  if (last_move_to_index_ < 0) {
    return GetPoint(~last_move_to_index_);
  } else {
    return GetPoint(last_move_to_index_);
  }
}

bool Path::IsFinite() const {
  ComputeBounds();
  return is_finite_;
}

bool Path::IsLine(Point* line) const {
  int verb_count = this->CountVerbs();

  if (2 == verb_count) {
    assert(verbs_.front() == Verb::kMove);
    if (verbs_[1] == Verb::kLine) {
      assert(2 == this->CountPoints());
      if (line) {
        const Point* pts = Points();
        line[0] = pts[0];
        line[1] = pts[1];
      }
    }
  }

  return false;
}

bool Path::operator==(const Path& other) const {
  return (this == std::addressof(other)) ||
         (last_move_to_index_ == other.last_move_to_index_ &&
          convexity_ == other.convexity_ && is_finite_ == other.is_finite_ &&
          points_.data() == other.points_.data() &&
          verbs_.data() == other.verbs_.data() &&
          conic_weights_.data() == other.conic_weights_.data());
}

void Path::Swap(Path& that) {
  if (this != &that) {
    std::swap(last_move_to_index_, that.last_move_to_index_);
    std::swap(convexity_, that.convexity_);
    std::swap(points_, that.points_);
    std::swap(verbs_, that.verbs_);
    std::swap(conic_weights_, that.conic_weights_);
    std::swap(is_finite_, that.is_finite_);
    std::swap(segment_masks_, that.segment_masks_);
  }
}

Path& Path::AddPath(const Path& src, float dx, float dy, AddMode mode) {
  Matrix matrix = Matrix::Translate(dx, dy);
  return AddPath(src, matrix, mode);
}

Path& Path::AddPath(const Path& src, AddMode mode) {
  return AddPath(src, Matrix(1.f), mode);
}

Path& Path::AddPath(const Path& src, const Matrix& matrix, AddMode mode) {
  if (src.IsEmpty()) {
    return *this;
  }

  if (mode == AddMode::kAppend) {
    if (src.last_move_to_index_ >= 0) {
      last_move_to_index_ =
          src.last_move_to_index_ + static_cast<int32_t>(CountPoints());
    } else {
      last_move_to_index_ =
          src.last_move_to_index_ - static_cast<int32_t>(CountVerbs());
    }

    // add verb
    verbs_.insert(verbs_.end(), src.verbs_.begin(), src.verbs_.end());
    segment_masks_ |= src.segment_masks_;
    // add weights
    conic_weights_.insert(conic_weights_.end(), src.conic_weights_.begin(),
                          src.conic_weights_.end());
    // add points
    //    points_.insert(points_.end(), src.points_.begin(), src.points_.end());
    if (points_.capacity() < points_.size() + src.points_.size()) {
      points_.reserve((points_.capacity() + src.points_.capacity()));
    }

    for (const auto& p : src.points_) {
      points_.emplace_back(matrix * p);
    }

    return *this;
  }

  RawIter iter{src};
  Point pts[4];
  Verb verb;
  bool first_verb = true;

  while ((verb = iter.Next(pts)) != Verb::kDone) {
    switch (verb) {
      case Verb::kMove:
        pts[0] = matrix * pts[0];
        if (first_verb && !IsEmpty()) {
          InjectMoveToIfNeed();
          Point last_pt;
          if (!this->GetLastPt(&last_pt) || last_pt != pts[0]) {
            LineTo(pts[0].x, pts[0].y);
          }
        } else {
          MoveTo(pts[0].x, pts[0].y);
        }
        break;
      case Verb::kLine:
        pts[1] = matrix * pts[1];
        LineTo(pts[1].x, pts[1].y);
        break;
      case Verb::kQuad:
        pts[1] = matrix * pts[1];
        pts[2] = matrix * pts[2];
        QuadTo(pts[1].x, pts[1].y, pts[2].x, pts[2].y);
        break;
      case Verb::kConic:
        pts[1] = matrix * pts[1];
        pts[2] = matrix * pts[2];
        ConicTo(pts[1].x, pts[1].y, pts[2].x, pts[2].y, iter.ConicWeight());
        break;
      case Verb::kCubic:
        pts[1] = matrix * pts[1];
        pts[2] = matrix * pts[2];
        pts[3] = matrix * pts[3];
        CubicTo(pts[1].x, pts[1].y, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
        break;
      case Verb::kClose:
        Close();
        break;
      default:
        break;
    }
    first_verb = false;
  }
  return *this;
}

void Path::SetLastPt(float x, float y) {
  if (CountPoints() == 0) {
    MoveTo(x, y);
  } else {
    PointSet(points_.back(), x, y);
  }
}

Path Path::CopyWithMatrix(const Matrix& matrix) const {
  Path ret;

  ret.last_move_to_index_ = last_move_to_index_;
  ret.convexity_ = convexity_;
  ret.first_direction_ = first_direction_;
  ret.fill_type_ = fill_type_;

  ret.points_.reserve(this->points_.capacity());
  for (const auto& p : this->points_) {
    ret.points_.emplace_back(matrix * p);
  }

  ret.conic_weights_ = conic_weights_;
  ret.verbs_ = verbs_;
  ret.segment_masks_ = segment_masks_;

  ret.is_finite_ = is_finite_;
  ret.bounds_ = bounds_;

  return ret;
}

Path Path::CopyWithScale(float scale) const {
  Path ret;

  ret.last_move_to_index_ = last_move_to_index_;
  ret.convexity_ = convexity_;
  ret.first_direction_ = first_direction_;
  ret.fill_type_ = fill_type_;

  ret.points_.reserve(this->points_.capacity());
  for (auto p : this->points_) {
    p.x *= scale;
    p.y *= scale;
    ret.points_.emplace_back(p);
  }

  ret.conic_weights_.resize(conic_weights_.size());
  std::memcpy(ret.conic_weights_.data(), conic_weights_.data(),
              conic_weights_.size() * sizeof(float));
  ret.verbs_.resize(verbs_.size());
  std::memcpy(ret.verbs_.data(), verbs_.data(), verbs_.size() * sizeof(Verb));
  ret.segment_masks_ = segment_masks_;

  ret.is_finite_ = is_finite_;
  ret.bounds_ = bounds_;

  return ret;
}

void Path::InjectMoveToIfNeed() {
  if (last_move_to_index_ < 0) {
    float x, y;
    if (CountVerbs() == 0) {
      x = y = 0;
    } else {
      Point const& pt = AtPoint(~last_move_to_index_);
      x = pt.x;
      y = pt.y;
    }
    MoveTo(x, y);
  }
}

void Path::ComputeBounds() const {
  is_finite_ = ComputePtBounds(std::addressof(bounds_), *this);
}

bool Path::HasOnlyMoveTos() const { return segment_masks_ == 0; }

bool Path::ComputePtBounds(Rect* bounds, const Path& ref) {
  return bounds->SetBoundsCheck(ref.points_.data(), ref.CountPoints());
}

bool Path::IsZeroLengthSincePoint(int startPtIndex) const {
  int32_t count = CountPoints() - startPtIndex;
  if (count < 2) {
    return true;
  }

  auto pts = points_.data() + startPtIndex;
  Point const& first = *pts;

  for (int32_t index = 1; index < count; index++) {
    if (first != pts[index]) {
      return false;
    }
  }
  return true;
}

static void append_params(std::ostream& os, const std::string& label,
                          const Point pts[], int count,
                          float conicWeight = -12345) {
  os << label << "(";
  for (int i = 0; i < count; i++) {
    const Point* point = pts + i;
    float x = point->x;
    float y = point->y;
    std::stringstream ss;
    ss << "( " << x << ", " << y << ")";
    os << ss.str();
  }
  if (conicWeight != -12345) {
    os << ", " << conicWeight;
  }
  os << ");";
  os << std::endl;
}

bool Path::IsRect(Rect* rect, bool* is_closed, Direction* direction) const {
  if (segment_masks_ != SegmentMask::kLine) {
    return false;
  }

  int32_t verb_cnt = CountVerbs();

  int32_t corners = 0;
  int32_t curr_verb = 0;
  const Point* first_pt = nullptr;
  const Point* last_pt = nullptr;
  Point first_corner;
  Point third_corner;
  const Point* pts = points_.data();
  Point line_start;
  Point close_xy;

  bool closed_or_moved = false;
  bool auto_close = false;
  // -1 to 3, -1 is uninitialized
  std::array<int32_t, 5> directions{-1, -1, -1, -1, -1};

  while (curr_verb < verb_cnt && (!auto_close)) {
    auto verb = verbs_[curr_verb];

    switch (verb) {
      case Verb::kClose:
        auto_close = true;
        [[fallthrough]];
      case Verb::kLine: {
        if (verb != Verb::kClose) {
          last_pt = pts;
        }
        Point line_end = verb == Verb::kClose ? *first_pt : *pts++;
        Vec2 line_delta = Vec2{line_end - line_start};
        if (!FloatNearlyZero(line_delta.x) && !FloatNearlyZero(line_delta.y)) {
          return false;  // not a straight line
        }
        if (!FloatIsFinite(line_delta.x) || !FloatIsFinite(line_delta.y)) {
          return false;  // contains NaN number
        }

        if (line_start == line_end) {
          break;  // single point on side OK
        }

        auto next_direction = PathPriv::RectMakeDir(line_delta.x, line_delta.y);

        if (0 == corners) {
          directions[0] = next_direction;
          corners = 1;
          closed_or_moved = false;
          line_start = line_end;
          break;
        }
        if (closed_or_moved) {
          return false;  // closed followed by a line
        }

        if (auto_close && next_direction == directions[0]) {
          break;  // colinear with first
        }

        closed_or_moved = auto_close;
        if (directions[corners - 1] == next_direction) {
          if (3 == corners && verb == Verb::kLine) {
            third_corner = line_end;
          }
          line_start = line_end;
          break;  // colinear segment
        }

        directions[corners++] = next_direction;

        // opposite lines must point in opposite directions;
        // xoring them should equal 2
        switch (corners) {
          case 2:
            first_corner = line_start;
            break;
          case 3:
            if ((directions[0] ^ directions[2]) != 2) {
              return false;
            }
            third_corner = line_end;
            break;
          case 4:
            if ((directions[1] ^ directions[3]) != 2) {
              return false;
            }
            break;
          default:
            return false;  // too many direction changes
        }
        line_start = line_end;
      } break;
      case Verb::kQuad:
      case Verb::kConic:
      case Verb::kCubic:
        return false;
      case Verb::kMove:
        if (!corners) {
          first_pt = pts;
        } else {
          close_xy = *first_pt - *last_pt;
          if (close_xy.x && close_xy.y) {
            return false;
          }
        }

        line_start = *pts++;
        closed_or_moved = true;
        break;
      default:
        // unexpected verb
        return false;
    }
    curr_verb++;
  }

  // success if 4 corners and first point equals last
  if (corners < 3 || corners > 4) {
    return false;
  }

  // check if close generates diagonal
  close_xy = *first_pt - *last_pt;
  if (close_xy.x && close_xy.y) {
    return false;
  }

  if (rect) {
    rect->Set(first_corner, third_corner);
  }

  if (is_closed) {
    *is_closed = auto_close;
  }

  if (direction) {
    *direction = directions[0] == ((directions[1] + 1) & 3) ? Direction::kCW
                                                            : Direction::kCCW;
  }

  return true;
}

bool Path::Contains(float x, float y) const {
  if (IsEmpty()) {
    return false;
  }

  auto rect = GetBounds();

  if (x < rect.Left() || x > rect.Right() || y < rect.Top() ||
      y > rect.Bottom()) {
    return false;
  }

  PathScanner scanner{x, y};

  scanner.VisitPath(*this, true);

  int32_t winding = scanner.GetWindingCount();
  int32_t on_curve_count = scanner.GetOnCurveCount();
  int32_t ray_intesects_vertex_count = scanner.GetRayIntersectsVertexCount();

  if (GetFillType() == PathFillType::kEvenOdd) {
    winding = (winding & 1);
  }

  if (winding != 0) {
    return true;
  }

  if ((on_curve_count & 1) || GetFillType() == PathFillType::kEvenOdd) {
    if ((on_curve_count & 1) != 0) {
      return true;
    }
  }

  if (GetFillType() == PathFillType::kEvenOdd &&
      scanner.GetWindingCount() > 0) {
    return (ray_intesects_vertex_count & 1) != 0;
  }

  return false;
}

void Path::Dump() {
  Iter iter{*this, false};

  Point pts[4];
  Verb verb;

  std::ostringstream res;

  while ((verb = iter.Next(pts)) != Verb::kDone) {
    switch (verb) {
      case Verb::kMove:
        append_params(res, "path.MoveTo", std::addressof(pts[0]), 1);
        break;
      case Verb::kLine:
        append_params(res, "path.LineTo", std::addressof(pts[1]), 1);
        break;
      case Verb::kQuad:
        append_params(res, "path.QuadTo", std::addressof(pts[1]), 2);
        break;
      case Verb::kConic:
        append_params(res, "path.ConicTo", std::addressof(pts[1]), 2,
                      iter.ConicWeight());
        break;
      case Verb::kCubic:
        append_params(res, "path.CubicTo", std::addressof(pts[1]), 3);
        break;
      case Verb::kClose:
        append_params(res, "path.close()", nullptr, 0);
        break;
      default:
        verb = Verb::kDone;
        break;
    }
  }
  LOGI("------ {}", res.str());
}

int Path::LeadingMoveToCount() const {
  int count = verbs_.size();
  for (int i = 0; i < count; i++) {
    if (verbs_[i] != Verb::kMove) {
      return i;
    }
  }
  return count;
}

static int sign(float x) { return x < 0; }
#define kValueNeverReturnedBySign 2

enum class DirChange {
  kUnknown,
  kLeft,
  kRight,
  kStraight,
  kBackwards,  // if double back, allow simple lines to be convex
  kInvalid
};

// only valid for a single contour
struct Convexicator {
  /** The direction returned is only valid if the path is determined convex */
  Path::Direction GetFirstDirection() const { return first_direction_; }

  void SetMovePt(const Vec2& pt) {
    first_pt_ = last_pt_ = pt;
    expected_dir_ = DirChange::kInvalid;
  }

  bool AddPt(const Vec2& pt) {
    if (last_pt_ == pt) {
      return true;
    }
    // should only be true for first non-zero vector after setMovePt was called.
    if (first_pt_ == last_pt_ && expected_dir_ == DirChange::kInvalid) {
      last_vec_ = pt - last_pt_;
      first_vec_ = last_vec_;
    } else if (!this->AddVec(pt - last_pt_)) {
      return false;
    }
    last_pt_ = pt;
    return true;
  }

  static Path::ConvexityType BySign(const Point points[], int count) {
    if (count <= 3) {
      // point, line, or triangle are always convex
      return Path::ConvexityType::kConvex;
    }

    const Point* last = points + count;
    Point currPt = *points++;
    Point firstPt = currPt;
    int dxes = 0;
    int dyes = 0;
    int lastSx = kValueNeverReturnedBySign;
    int lastSy = kValueNeverReturnedBySign;
    for (int outerLoop = 0; outerLoop < 2; ++outerLoop) {
      while (points != last) {
        Vec4 vec = *points - currPt;
        if (!PointIsZero(vec)) {
          // give up if vector construction failed
          if (!PointIsFinite(vec)) {
            return Path::ConvexityType::kUnknown;
          }
          int sx = sign(vec.x);
          int sy = sign(vec.y);
          dxes += (sx != lastSx);
          dyes += (sy != lastSy);
          if (dxes > 3 || dyes > 3) {
            return Path::ConvexityType::kConcave;
          }
          lastSx = sx;
          lastSy = sy;
        }
        currPt = *points++;
        if (outerLoop) {
          break;
        }
      }
      points = &firstPt;
    }
    return Path::ConvexityType::kConvex;  // that is, it may be convex, don't
                                          // know yet
  }

  bool Close() {
    // If this was an explicit close, there was already a lineTo to fFirstPoint,
    // so this addPt() is a no-op. Otherwise, the addPt implicitly closes the
    // contour. In either case, we have to check the direction change along the
    // first vector in case it is concave.
    return this->AddPt(first_pt_) && this->AddVec(first_vec_);
  }

  bool IsFinite() const { return is_finite_; }

  int Reversals() const { return reversals_; }

 private:
  DirChange DirectionChange(const Vec2& cur_vec) {
    float cross = Vec2::Cross(last_vec_, cur_vec);
    if (glm::isinf(cross)) {
      return DirChange::kUnknown;
    }
    if (cross == 0) {
      return Vec2::Dot(last_vec_, cur_vec) < 0 ? DirChange::kBackwards
                                               : DirChange::kStraight;
    }
    return 1 == FloatSignAsInt(cross) ? DirChange::kRight : DirChange::kLeft;
  }

  bool AddVec(const Vec2& cur_vec) {
    DirChange dir = this->DirectionChange(cur_vec);
    switch (dir) {
      case DirChange::kLeft:  // fall through
      case DirChange::kRight:
        if (DirChange::kInvalid == expected_dir_) {
          expected_dir_ = dir;
          first_direction_ = (DirChange::kRight == dir) ? Path::Direction::kCW
                                                        : Path::Direction::kCCW;
        } else if (dir != expected_dir_) {
          first_direction_ = Path::Direction::kUnknown;
          return false;
        }
        last_vec_ = cur_vec;
        break;
      case DirChange::kStraight:
        break;
      case DirChange::kBackwards:
        //  allow path to reverse direction twice
        //    Given path.moveTo(0, 0); path.lineTo(1, 1);
        //    - 1st reversal: direction change formed by line (0,0 1,1), line
        //    (1,1 0,0)
        //    - 2nd reversal: direction change formed by line (1,1 0,0), line
        //    (0,0 1,1)
        last_vec_ = cur_vec;
        return ++reversals_ < 3;
      case DirChange::kUnknown:
        return (is_finite_ = false);
      case DirChange::kInvalid:
        break;
    }
    return true;
  }

  Vec2 first_pt_{0, 0};   // The first point of the contour, e.g. moveTo(x,y)
  Vec2 first_vec_{0, 0};  // The direction leaving first_pt_ to the next vertex

  Vec2 last_pt_{0, 0};   // The last point passed to addPt()
  Vec2 last_vec_{0, 0};  // The direction that brought the path to last_pt_

  DirChange expected_dir_{DirChange::kInvalid};
  Path::Direction first_direction_{Path::Direction::kUnknown};
  int reversals_{0};
  bool is_finite_{true};
};

constexpr static int PointsInVerb(Path::Verb verb) {
  switch (verb) {
    case Path::Verb::kMove:
      return 1;
    case Path::Verb::kLine:
      return 1;
    case Path::Verb::kQuad:
      return 2;
    case Path::Verb::kConic:
      return 2;
    case Path::Verb::kCubic:
      return 3;
    case Path::Verb::kClose:
      return 0;
    default:
      break;
  }
  return 0;
}

Path::ConvexityType Path::ComputeConvexity() const {
  int point_count = this->points_.size();
  int skip_count = LeadingMoveToCount() - 1;

  if (last_move_to_index_ >= 0) {
    if (last_move_to_index_ == static_cast<int32_t>(points_.size() - 1)) {
      for (int i = verbs_.size() - 1; i >= 0; i--) {
        if (verbs_[i] == Verb::kMove) {
          point_count--;
        }
      }
    } else if (last_move_to_index_ != skip_count) {
      return Path::ConvexityType::kConcave;
    }
  }

  const Point* points = points_.data();
  if (skip_count > 0) {
    points += skip_count;
    point_count -= skip_count;
  }

  auto convexity = Convexicator::BySign(points, point_count);
  if (convexity == Path::ConvexityType::kConcave) {
    return convexity;
  }

  int contour_count = 0;
  bool needs_close = false;
  Convexicator state;

  Path::Iter iter{*this, false};
  std::array<Point, 4> pts = {};
  for (;;) {
    Path::Verb verb = iter.Next(pts.data());
    if (verb == Path::Verb::kDone) {
      break;
    }

    if (contour_count == 0) {
      if (verb == Path::Verb::kMove) {
        state.SetMovePt(FromPoint(pts[0]));
      } else {
        contour_count++;
        needs_close = true;
      }
    }

    if (contour_count == 1) {
      if (verb == Path::Verb::kMove || verb == Path::Verb::kClose) {
        if (!state.Close()) {
          return Path::ConvexityType::kConcave;
        }
        needs_close = false;
        contour_count++;
      } else {
        int count = PointsInVerb(verb);
        for (int i = 1; i <= count; i++) {
          if (!state.AddPt(FromPoint(pts[i]))) {
            return Path::ConvexityType::kConcave;
          }
        }
      }
    } else {
      // The first contour has closed and anything other than spurious trailing
      // moves means there's multiple contours and the path can't be convex
      if (verb != Path::Verb::kMove) {
        return Path::ConvexityType::kConcave;
      }
    }
  }

  if (needs_close && !state.Close()) {
    return Path::ConvexityType::kConcave;
  }
  first_direction_ = state.GetFirstDirection();
  return Path::ConvexityType::kConvex;
}

}  // namespace skity
