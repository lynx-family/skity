// Copyright 2008 The Android Open Source Project

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/geometry/stroke.hpp>

#include "src/geometry/conic.hpp"
#include "src/geometry/cubic.hpp"
#include "src/geometry/point_priv.hpp"

namespace skity {

static constexpr int kRecursiveLimits = 11 * 3;

// Utilities
namespace {

bool has_valid_tangent(const Path::Iter* iter) {
  Path::Iter copy = *iter;
  Point pts[4];
  for (;;) {
    Path::Verb verb = copy.Next(pts);
    switch (verb) {
      case Path::Verb::kMove:
        return false;
      case Path::Verb::kLine:
        if (pts[0] == pts[1]) {
          continue;
        }
        return true;
      case Path::Verb::kQuad:
      case Path::Verb::kConic:
        if (pts[0] == pts[1] && pts[0] == pts[2]) {
          continue;
        }
      case Path::Verb::kCubic:
        if (pts[0] == pts[1] && pts[0] == pts[2] && pts[0] == pts[3]) {
          continue;
        }
        return true;
      case Path::Verb::kClose:
      case Path::Verb::kDone:
        return false;
    }
  }
}

/* Given quad, see if all there points are in a line.
   Return true if the inside point is close to a line connecting the outermost
   points.

   Find the outermost point by looking for the largest difference in X or Y.
   Since the XOR of the indices is 3  (0 ^ 1 ^ 2)
   the missing index equals: outer_1 ^ outer_2 ^ 3
 */
bool quad_in_line(const Point quad[3]) {
  float ptMax = -1;
  int outer1 = 0;
  int outer2 = 0;
  for (int index = 0; index < 2; ++index) {
    for (int inner = index + 1; inner < 3; ++inner) {
      Vector testDiff = quad[inner] - quad[index];
      float testMax = std::max(std::fabs(testDiff.x), std::fabs(testDiff.y));
      if (ptMax < testMax) {
        outer1 = index;
        outer2 = inner;
        ptMax = testMax;
      }
    }
  }
  int mid = outer1 ^ outer2 ^ 3;
  const float kCurvatureSlop =
      0.000005f;  // this multiplier is pulled out of the air
  float lineSlop = ptMax * ptMax * kCurvatureSlop;
  return pt_to_line(quad[mid], quad[outer1], quad[outer2]) <= lineSlop;
}

float find_quad_max_curvature(const Point src[3]) {
  float Ax = src[1].x - src[0].x;
  float Ay = src[1].y - src[0].y;
  float Bx = src[0].x - src[1].x - src[1].x + src[2].x;
  float By = src[0].y - src[1].y - src[1].y + src[2].y;

  float numer = -(Ax * Bx + Ay * By);
  float denom = Bx * Bx + By * By;
  if (denom < 0) {
    numer = -numer;
    denom = -denom;
  }
  if (numer <= 0) {
    return 0;
  }
  if (numer >= denom) {  // Also catches denom=0.
    return 1;
  }
  float t = numer / denom;
  return t;
}

bool set_normal_unitnormal(const Point& before, const Point& after, float scale,
                           float radius, Vector* normal, Vector* unitNormal) {
  if (!VectorSetNormal(*unitNormal, (after.x - before.x) * scale,
                       (after.y - before.y) * scale)) {
    return false;
  }
  PointRotateCCW(unitNormal);
  PointScale(*unitNormal, radius, normal);
  return true;
}

// Intersect the line with the quad and return the t values on the quad where
// the line crosses.
int intersect_quad_ray(const Point line[2], const Point quad[3],
                       float roots[2]) {
  Vector vec = line[1] - line[0];
  float r[3];
  for (int n = 0; n < 3; ++n) {
    r[n] = (quad[n].y - line[0].y) * vec.x - (quad[n].x - line[0].x) * vec.y;
  }
  float A = r[2];
  float B = r[1];
  float C = r[0];
  A += C - 2 * B;  // A = a - 2*b + c
  B -= C;          // B = -(b - c)
  return FindUnitQuadRoots(A, 2 * B, C, roots);
}

bool points_within_dist(const Point& nearPt, const Point& farPt, float limit) {
  return PointDistanceToSqd(nearPt, farPt) <= limit * limit;
}

bool sharp_angle(const Point quad[3]) {
  Vector smaller = quad[1] - quad[0];
  Vector larger = quad[1] - quad[2];
  float smallerLen = PointLengthSqd(smaller);
  float largerLen = PointLengthSqd(larger);
  if (smallerLen > largerLen) {
    using std::swap;
    swap(smaller, larger);
    largerLen = smallerLen;
  }
  if (!PointSetLength<false>(smaller, smaller.x, smaller.y, largerLen)) {
    return false;
  }
  float dot = VectorDotProduct(smaller, larger);
  return dot > 0;
}

}  // namespace

// Cap and Join
namespace {
typedef void (*CapProc)(Path* path, const Point& pivot, const Vector& normal,
                        const Point& stop);

typedef void (*JoinProc)(Path* outer, Path* inner,
                         const Vector& beforeUnitNormal, const Point& pivot,
                         const Vector& afterUnitNormal, float radius,
                         float miter_limit);

void ButtCapper(Path* path, const Point& pivot, const Vector& normal,
                const Point& stop) {
  path->LineTo(stop.x, stop.y);
}

void SquareCapper(Path* path, const Point& pivot, const Vector& normal,
                  const Point& stop) {
  Point parallel;
  PointRotateCW(normal, &parallel);
  path->LineTo(pivot.x + normal.x + parallel.x,
               pivot.y + normal.y + parallel.y);
  path->LineTo(pivot.x - normal.x + parallel.x,
               pivot.y - normal.y + parallel.y);
  path->LineTo(stop.x, stop.y);
}

void RoundCapper(Path* path, const Point& pivot, const Vector& normal,
                 const Point& stop) {
  Point parallel;
  PointRotateCW(normal, &parallel);

  Point projectedCenter = pivot + parallel;

  path->ConicTo(projectedCenter + normal, projectedCenter, FloatRoot2Over2);
  path->ConicTo(projectedCenter - normal, stop, FloatRoot2Over2);
}

CapProc CapFactory(Paint::Cap cap) {
  const CapProc gCappers[] = {ButtCapper, RoundCapper, SquareCapper};

  return gCappers[cap];
}

bool is_clockwise(const Vector& before, const Vector& after) {
  return before.x * after.y > before.y * after.x;
}

void handle_inner_join(Path* inner, const Point& pivot, const Vector& after) {
  inner->LineTo(pivot.x, pivot.y);
  inner->LineTo(pivot.x - after.x, pivot.y - after.y);
}

void BevelJoiner(Path* outer, Path* inner, const Vector& beforeUnitNormal,
                 const Point& pivot, const Vector& afterUnitNormal,
                 float radius, float miter_limit) {
  Vector after;
  PointScale(afterUnitNormal, radius, &after);

  if (!is_clockwise(beforeUnitNormal, afterUnitNormal)) {
    using std::swap;
    swap(outer, inner);
    after.x = -after.x;
    after.y = -after.y;
  }

  outer->LineTo(pivot.x + after.x, pivot.y + after.y);
  handle_inner_join(inner, pivot, after);
}

enum AngleType {
  kNearly180_AngleType,
  kSharp_AngleType,
  kShallow_AngleType,
  kNearlyLine_AngleType
};

AngleType Dot2AngleType(float dot) {
  if (dot >= 0) {  // shallow or line
    return FloatNearlyZero(1.f - dot) ? kNearlyLine_AngleType
                                      : kShallow_AngleType;
  } else {  // sharp or 180
    return FloatNearlyZero(1.f + dot) ? kNearly180_AngleType : kSharp_AngleType;
  }
}

void RoundJoiner(Path* outer, Path* inner, const Vector& beforeUnitNormal,
                 const Point& pivot, const Vector& afterUnitNormal,
                 float radius, float miter_limit) {
  float dotProd = VectorDotProduct(beforeUnitNormal, afterUnitNormal);
  AngleType angleType = Dot2AngleType(dotProd);

  if (angleType == kNearlyLine_AngleType) return;

  Vector before = beforeUnitNormal;
  Vector after = afterUnitNormal;
  RotationDirection dir = RotationDirection::kCW;

  if (!is_clockwise(before, after)) {
    using std::swap;
    swap(outer, inner);
    before.x = -before.x;
    before.y = -before.y;
    after.x = -after.x;
    after.y = -after.y;
    dir = RotationDirection::kCCW;
  }

  Matrix matrix =
      Matrix::Translate(pivot.x, pivot.y) * Matrix::Scale(radius, radius);
  Conic conics[Conic::kMaxConicsForArc];
  int count = Conic::BuildUnitArc(before, after, dir, &matrix, conics);
  if (count > 0) {
    for (int i = 0; i < count; ++i) {
      outer->ConicTo(conics[i].pts[1], conics[i].pts[2], conics[i].w);
    }
    PointScale(after, radius, &after);
    handle_inner_join(inner, pivot, after);
  }
}

void MiterJoiner(Path* outer, Path* inner, const Vector& beforeUnitNormal,
                 const Point& pivot, const Vector& afterUnitNormal,
                 float radius, float miter_limit) {
  float dotProd = VectorDotProduct(beforeUnitNormal, afterUnitNormal);
  AngleType angleType = Dot2AngleType(dotProd);
  Vector before = beforeUnitNormal;
  Vector after = afterUnitNormal;
  Vector mid;

  if (angleType == kNearlyLine_AngleType) {
    return;
  }
  if (angleType == kNearly180_AngleType) {
    PointScale(after, radius, &after);
    outer->LineTo(pivot.x + after.x, pivot.y + after.y);
    handle_inner_join(inner, pivot, after);
    return;
  }

  if (!is_clockwise(before, after)) {
    using std::swap;
    swap(outer, inner);
    before.x = -before.x;
    before.y = -before.y;
    after.x = -after.x;
    after.y = -after.y;
  }

  // right angle
  if (0 == dotProd && miter_limit >= FloatSqrt2) {
    mid = (before + after) * radius;
  } else {
    Vector out_dir = before + after;
    float k = 2.f / (out_dir.x * out_dir.x + out_dir.y * out_dir.y);
    mid = k * out_dir * radius;

    if (glm::length(Vec2{mid.x, mid.y}) >= miter_limit * radius) {
      PointScale(after, radius, &after);
      outer->LineTo(pivot.x + after.x, pivot.y + after.y);
      handle_inner_join(inner, pivot, after);
      return;
    }
  }

  // miter case
  outer->LineTo(pivot.x + mid.x, pivot.y + mid.y);
  PointScale(after, radius, &after);
  outer->LineTo(pivot.x + after.x, pivot.y + after.y);
  handle_inner_join(inner, pivot, after);
  return;
}

JoinProc JoinFactory(Paint::Join join) {
  const JoinProc gJoiners[] = {MiterJoiner, RoundJoiner, BevelJoiner};

  return gJoiners[join];
}

}  // namespace

struct QuadConstruct {
  Point fQuad[3];       // the stroked quad parallel to the original curve
  Point fTangentStart;  // a point tangent to fQuad[0]
  Point fTangentEnd;    // a point tangent to fQuad[2]
  float fStartT;        // a segment of the original curve
  float fMidT;          //              "
  float fEndT;          //              "
  bool fStartSet;       // state to share common points across structs
  bool fEndSet;         //

  // return false if start and end are too close to have a unique middle
  bool init(float start, float end) {
    fStartT = start;
    fMidT = (start + end) * 0.5f;
    fEndT = end;
    fStartSet = fEndSet = false;
    return fStartT < fMidT && fMidT < fEndT;
  }

  bool initWithStart(QuadConstruct* parent) {
    if (!init(parent->fStartT, parent->fMidT)) {
      return false;
    }
    fQuad[0] = parent->fQuad[0];
    fTangentStart = parent->fTangentStart;
    fStartSet = true;
    return true;
  }

  bool initWithEnd(QuadConstruct* parent) {
    if (!init(parent->fMidT, parent->fEndT)) {
      return false;
    }
    fQuad[2] = parent->fQuad[2];
    fTangentEnd = parent->fTangentEnd;
    fEndSet = true;
    return true;
  }
};

class PathStroker {
 public:
  PathStroker(const Path& src, float radius, float miter_limit, Paint::Cap cap,
              Paint::Join join, float scale);

  void MoveTo(const Point&);
  void LineTo(const Point&, const Path::Iter* iter = nullptr);
  void QuadTo(const Point&, const Point&);
  void Close(bool isLine) { this->FinishContour(true, isLine); }

  void Done(Path* dst, bool isLine) {
    this->FinishContour(false, isLine);
    dst->Swap(outer_path_);
  }

 private:
  float radius_;
  float scale_;
  float miter_limit_;
  float fInvResScale;
  float fInvResScaleSquared;

  Path outer_path_;
  Path inner_path_;
  Vector fFirstNormal, fPrevNormal, fFirstUnitNormal, fPrevUnitNormal;
  Point fFirstPt, fPrevPt;  // on original path
  Point fFirstOuterPt;
  int fFirstOuterPtIndexInContour;
  int fSegmentCount;
  bool fJoinCompleted;  // previous join was not degenerate

  CapProc cap_proc_;
  JoinProc join_proc_;

  int fRecursionDepth;  // track stack depth to abort if numerics run amok

  enum StrokeType {
    kOuter_StrokeType =
        1,  // use sign-opposite values later to flip perpendicular axis
    kInner_StrokeType = -1
  } fStrokeType;

  enum ResultType {
    kSplit_ResultType,       // the caller should split the quad stroke in two
    kDegenerate_ResultType,  // the caller should add a line
    kQuad_ResultType,  // the caller should (continue to try to) add a quad
                       // stroke
  };

  enum ReductionType {
    kPoint_ReductionType,  // all curve points are practically identical
    kLine_ReductionType,   // the control point is on the line between the ends
    kQuad_ReductionType,   // the control point is outside the line between the
                           // ends
    kDegenerate_ReductionType,  // the control point is on the line but outside
                                // the ends
  };

  void Init(StrokeType strokeType, QuadConstruct*, float tStart, float tEnd);
  void FinishContour(bool close, bool isLine);

  bool PreJoinTo(const Point&, Vector* normal, Vector* unitNormal, bool isLine);
  void PostJoinTo(const Point&, const Vector& normal, const Vector& unitNormal);

  void SetQuadEndNormal(const Point quad[3], const Vector& normalAB,
                        const Vector& unitNormalAB, Vector* normalBC,
                        Vector* unitNormalBC);
  bool QuadStroke(const Point quad[3], QuadConstruct*);
  ResultType CompareQuadQuad(const Point quad[3], QuadConstruct*);
  void QuadPerpRay(const Point quad[3], float t, Point* tPt, Point* onPt,
                   Point* tangent) const;
  void SetRayPts(const Point& tPt, Vector* dxy, Point* onPt,
                 Point* tangent) const;
  ResultType IntersectRay(QuadConstruct*, int) const;
  ResultType StrokeCloseEnough(const Point stroke[3], const Point ray[2],
                               QuadConstruct*, int depth) const;
  bool PtInQuadBounds(const Point quad[3], const Point& pt) const;

  void RawLineTo(const Point& currPt, const Vector& normal);

  static ReductionType CheckQuadLinear(const Point quad[3], Point* reduction);
};

void PathStroker::MoveTo(const Point& pt) {
  if (fSegmentCount > 0) {
    this->FinishContour(false, false);
  }
  fSegmentCount = 0;
  fFirstPt = fPrevPt = pt;
  fJoinCompleted = false;
}

void PathStroker::Init(StrokeType strokeType, QuadConstruct* quadPts,
                       float tStart, float tEnd) {
  fStrokeType = strokeType;
  quadPts->init(tStart, tEnd);
}

void PathStroker::LineTo(const Point& currPt, const Path::Iter* iter) {
  bool teenyLine =
      PointEqualsWithinTolerance(fPrevPt, currPt, NearlyZero * fInvResScale);
  if (teenyLine && cap_proc_ == ButtCapper) {
    return;
  }
  if (teenyLine && (fJoinCompleted || (iter && has_valid_tangent(iter)))) {
    return;
  }
  Vector normal, unitNormal;

  if (!this->PreJoinTo(currPt, &normal, &unitNormal, true)) {
    return;
  }
  this->RawLineTo(currPt, normal);
  this->PostJoinTo(currPt, normal, unitNormal);
}

void PathStroker::QuadTo(const Point& c, const Point& p2) {
  const Point quad[3] = {fPrevPt, c, p2};
  Point reduction;
  ReductionType reductionType = CheckQuadLinear(quad, &reduction);
  if (kPoint_ReductionType == reductionType) {
    /* If the stroke consists of a moveTo followed by a degenerate curve, treat
       it as if it were followed by a zero-length line. Lines without length can
       have square and round end caps. */
    this->LineTo(p2);
    return;
  }
  if (kLine_ReductionType == reductionType) {
    this->LineTo(p2);
    return;
  }
  if (kDegenerate_ReductionType == reductionType) {
    this->LineTo(reduction);
    JoinProc saveJoiner = join_proc_;
    join_proc_ = RoundJoiner;
    this->LineTo(p2);
    join_proc_ = saveJoiner;
    return;
  }

  Vector normalAB, unitAB, normalBC, unitBC;
  if (!this->PreJoinTo(c, &normalAB, &unitAB, false)) {
    this->LineTo(p2);
    return;
  }
  QuadConstruct quadPts;
  this->Init(kOuter_StrokeType, &quadPts, 0, 1);
  (void)this->QuadStroke(quad, &quadPts);
  this->Init(kInner_StrokeType, &quadPts, 0, 1);
  (void)this->QuadStroke(quad, &quadPts);
  this->SetQuadEndNormal(quad, normalAB, unitAB, &normalBC, &unitBC);

  this->PostJoinTo(p2, normalBC, unitBC);
}

void PathStroker::SetQuadEndNormal(const Point quad[3], const Vector& normalAB,
                                   const Vector& unitNormalAB, Vector* normalBC,
                                   Vector* unitNormalBC) {
  if (!set_normal_unitnormal(quad[1], quad[2], scale_, radius_, normalBC,
                             unitNormalBC)) {
    *normalBC = normalAB;
    *unitNormalBC = unitNormalAB;
  }
}

bool PathStroker::QuadStroke(const Point quad[3], QuadConstruct* quadPts) {
  ResultType resultType = this->CompareQuadQuad(quad, quadPts);
  if (kQuad_ResultType == resultType) {
    const Point* stroke = quadPts->fQuad;
    Path* path = fStrokeType == kOuter_StrokeType ? &outer_path_ : &inner_path_;
    path->QuadTo(stroke[1].x, stroke[1].y, stroke[2].x, stroke[2].y);
    return true;
  }
  if (kDegenerate_ResultType == resultType) {
    // addDegenerateLine(quadPts);
    return true;
  }

  if (++fRecursionDepth > kRecursiveLimits) {
    return false;  // just abort if projected quad isn't representable
  }
  QuadConstruct half;
  (void)half.initWithStart(quadPts);
  if (!this->QuadStroke(quad, &half)) {
    return false;
  }
  (void)half.initWithEnd(quadPts);
  if (!this->QuadStroke(quad, &half)) {
    return false;
  }
  --fRecursionDepth;
  return true;
}

PathStroker::ResultType PathStroker::CompareQuadQuad(const Point quad[3],
                                                     QuadConstruct* quadPts) {
  // get the quadratic approximation of the stroke
  if (!quadPts->fStartSet) {
    Point quadStartPt;
    this->QuadPerpRay(quad, quadPts->fStartT, &quadStartPt, &quadPts->fQuad[0],
                      &quadPts->fTangentStart);
    quadPts->fStartSet = true;
  }
  if (!quadPts->fEndSet) {
    Point quadEndPt;
    this->QuadPerpRay(quad, quadPts->fEndT, &quadEndPt, &quadPts->fQuad[2],
                      &quadPts->fTangentEnd);
    quadPts->fEndSet = true;
  }
  ResultType resultType = this->IntersectRay(quadPts, fRecursionDepth);
  if (resultType != kQuad_ResultType) {
    return resultType;
  }
  // project a ray from the curve to the stroke
  Point ray[2];
  this->QuadPerpRay(quad, quadPts->fMidT, &ray[1], &ray[0], nullptr);
  return this->StrokeCloseEnough(quadPts->fQuad, ray, quadPts, fRecursionDepth);
}

// Given a quad and t, return the point on curve, its perpendicular, and the
// perpendicular tangent.
void PathStroker::QuadPerpRay(const Point quad[3], float t, Point* tPt,
                              Point* onPt, Point* tangent) const {
  Vector dxy;
  QuadCoeff::EvalQuadAt(std::array<Point, 3>{quad[0], quad[1], quad[2]}, t, tPt,
                        &dxy);
  if (dxy.x == 0 && dxy.y == 0) {
    dxy = quad[2] - quad[0];
  }
  SetRayPts(*tPt, &dxy, onPt, tangent);
}

void PathStroker::SetRayPts(const Point& tPt, Vector* dxy, Point* onPt,
                            Point* tangent) const {
  if (!PointSetLength<false>(*dxy, dxy->x, dxy->y, radius_)) {
    dxy->x = radius_;
    dxy->y = 0;
  }
  float axisFlip =
      static_cast<float>(fStrokeType);  // go opposite ways for outer, inner
  onPt->x = tPt.x + axisFlip * dxy->y;
  onPt->y = tPt.y - axisFlip * dxy->x;
  if (tangent) {
    tangent->x = onPt->x + dxy->x;
    tangent->y = onPt->y + dxy->y;
  }
}

// Find the intersection of the stroke tangents to construct a stroke quad.
// Return whether the stroke is a degenerate (a line), a quad, or must be split.
// Optionally compute the quad's control point.
PathStroker::ResultType PathStroker::IntersectRay(QuadConstruct* quadPts,
                                                  int depth) const {
  const Point& start = quadPts->fQuad[0];
  const Point& end = quadPts->fQuad[2];
  Vector aLen = quadPts->fTangentStart - start;
  Vector bLen = quadPts->fTangentEnd - end;
  /* Slopes match when denom goes to zero:
                    axLen / ayLen ==                   bxLen / byLen
  (ayLen * byLen) * axLen / ayLen == (ayLen * byLen) * bxLen / byLen
           byLen  * axLen         ==  ayLen          * bxLen
           byLen  * axLen         -   ayLen          * bxLen         ( == denom
  )
   */
  float denom = VectorCrossProduct(aLen, bLen);

  if (denom == 0 || denom == std::numeric_limits<float>::infinity()) {
    return kDegenerate_ResultType;
  }
  Vector ab0 = start - end;
  float numerA = VectorCrossProduct(bLen, ab0);
  float numerB = VectorCrossProduct(aLen, ab0);
  if ((numerA >= 0) ==
      (numerB >= 0)) {  // if the control point is outside the quad ends
    // if the perpendicular distances from the quad points to the opposite
    // tangent line are small, a straight line is good enough
    float dist1 = pt_to_line(start, end, quadPts->fTangentEnd);
    float dist2 = pt_to_line(end, start, quadPts->fTangentStart);
    if (std::max(dist1, dist2) <= fInvResScaleSquared) {
      return kDegenerate_ResultType;
    }
    return kSplit_ResultType;
  }
  // check to see if the denominator is teeny relative to the numerator
  // if the offset by one will be lost, the ratio is too large
  numerA /= denom;
  bool validDivide = numerA > numerA - 1;
  if (validDivide) {
    Point* ctrlPt = &quadPts->fQuad[1];
    // the intersection of the tangents need not be on the tangent segment
    // so 0 <= numerA <= 1 is not necessarily true
    ctrlPt->x = start.x * (1 - numerA) + quadPts->fTangentStart.x * numerA;
    ctrlPt->y = start.y * (1 - numerA) + quadPts->fTangentStart.y * numerA;
    return kQuad_ResultType;
  }
  // if the lines are parallel, straight line is good enough
  return kDegenerate_ResultType;
}

PathStroker::ResultType PathStroker::StrokeCloseEnough(const Point stroke[3],
                                                       const Point ray[2],
                                                       QuadConstruct* quadPts,
                                                       int depth) const {
  Point strokeMid = QuadCoeff::EvalQuadAt(
      std::array<Point, 3>{stroke[0], stroke[1], stroke[2]}, 0.5f);
  // measure the distance from the curve to the quad-stroke midpoint, compare to
  // radius
  if (points_within_dist(ray[0], strokeMid,
                         fInvResScale)) {  // if the difference is small
    if (sharp_angle(quadPts->fQuad)) {
      return kSplit_ResultType;
    }
    return kQuad_ResultType;
  }
  // measure the distance to quad's bounds (quick reject)
  // an alternative : look for point in triangle
  if (!PtInQuadBounds(stroke, ray[0])) {  // if far, subdivide
    return kSplit_ResultType;
  }
  // measure the curve ray distance to the quad-stroke
  float roots[2];
  int rootCount = intersect_quad_ray(ray, stroke, roots);
  if (rootCount != 1) {
    return kSplit_ResultType;
  }
  Point quadPt = QuadCoeff::EvalQuadAt(
      std::array<Point, 3>{stroke[0], stroke[1], stroke[2]}, roots[0]);
  float error = fInvResScale * (1.f - std::abs(roots[0] - 0.5f) * 2);
  if (points_within_dist(ray[0], quadPt,
                         error)) {  // if the difference is small, we're done
    if (sharp_angle(quadPts->fQuad)) {
      return kSplit_ResultType;
    }
    return kQuad_ResultType;
  }
  // otherwise, subdivide
  return kSplit_ResultType;
}

bool PathStroker::PtInQuadBounds(const Point quad[3], const Point& pt) const {
  float xMin = std::min(std::min(quad[0].x, quad[1].x), quad[2].x);
  if (pt.x + fInvResScale < xMin) {
    return false;
  }
  float xMax = std::max(std::max(quad[0].x, quad[1].x), quad[2].x);
  if (pt.x - fInvResScale > xMax) {
    return false;
  }
  float yMin = std::min(std::min(quad[0].y, quad[1].y), quad[2].y);
  if (pt.y + fInvResScale < yMin) {
    return false;
  }
  float yMax = std::max(std::max(quad[0].y, quad[1].y), quad[2].y);
  if (pt.y - fInvResScale > yMax) {
    return false;
  }
  return true;
}

void PathStroker::FinishContour(bool close, bool isLine) {
  if (fSegmentCount > 0) {
    Point pt;

    if (close) {
      join_proc_(&outer_path_, &inner_path_, fPrevUnitNormal, fPrevPt,
                 fFirstUnitNormal, radius_, miter_limit_);
      outer_path_.Close();

      bool fCanIgnoreCenter = false;
      if (fCanIgnoreCenter) {
        // If we can ignore the center just make sure the larger of the two
        // paths is preserved and don't add the smaller one.
        if (inner_path_.GetBounds().Contains(outer_path_.GetBounds())) {
          inner_path_.Swap(outer_path_);
        }
      } else {
        // now add fInner as its own contour
        inner_path_.GetLastPt(&pt);
        outer_path_.MoveTo(pt.x, pt.y);
        outer_path_.ReversePathTo(inner_path_);
        outer_path_.Close();
      }
    } else {  // add caps to start and end
      // cap the end
      inner_path_.GetLastPt(&pt);
      cap_proc_(&outer_path_, fPrevPt, fPrevNormal, pt);
      outer_path_.ReversePathTo(inner_path_);
      // cap the start
      cap_proc_(&outer_path_, fFirstPt, -fFirstNormal, fFirstOuterPt);
      outer_path_.Close();
    }
  }
  inner_path_.Reset();
  fSegmentCount = -1;
  fFirstOuterPtIndexInContour = outer_path_.CountPoints();
}

bool PathStroker::PreJoinTo(const Point& currPt, Vector* normal,
                            Vector* unitNormal, bool isLine) {
  float prevX = fPrevPt.x;
  float prevY = fPrevPt.y;

  if (!set_normal_unitnormal(fPrevPt, currPt, scale_, radius_, normal,
                             unitNormal)) {
    if (cap_proc_ == ButtCapper) {
      return false;
    }
    normal->x = radius_;
    normal->y = 0;
    unitNormal->x = 1;
    unitNormal->y = 0;
  }

  if (fSegmentCount == 0) {
    fFirstNormal = *normal;
    fFirstUnitNormal = *unitNormal;
    fFirstOuterPt.x = prevX + normal->x;
    fFirstOuterPt.y = prevY + normal->y;

    outer_path_.MoveTo(fFirstOuterPt.x, fFirstOuterPt.y);
    inner_path_.MoveTo(prevX - normal->x, prevY - normal->y);
  } else {  // we have a previous segment
    join_proc_(&outer_path_, &inner_path_, fPrevUnitNormal, fPrevPt,
               *unitNormal, radius_, miter_limit_);
  }
  return true;
}

void PathStroker::PostJoinTo(const Point& currPt, const Vector& normal,
                             const Vector& unitNormal) {
  fJoinCompleted = true;
  fPrevPt = currPt;
  fPrevUnitNormal = unitNormal;
  fPrevNormal = normal;
  fSegmentCount += 1;
}

void PathStroker::RawLineTo(const Point& currPt, const Vector& normal) {
  outer_path_.LineTo(currPt.x + normal.x, currPt.y + normal.y);
  inner_path_.LineTo(currPt.x - normal.x, currPt.y - normal.y);
}

PathStroker::ReductionType PathStroker::CheckQuadLinear(const Point quad[3],
                                                        Point* reduction) {
  Vector p1_to_c = quad[1] - quad[0];
  bool degenerateP1C = !CanNormalize(p1_to_c.x, p1_to_c.y);
  Vector c_to_p2 = quad[2] - quad[1];
  bool degenerateCP2 = !CanNormalize(c_to_p2.x, c_to_p2.y);
  if (degenerateP1C & degenerateCP2) {
    return kPoint_ReductionType;
  }
  if (degenerateP1C | degenerateCP2) {
    return kLine_ReductionType;
  }
  if (!quad_in_line(quad)) {
    return kQuad_ReductionType;
  }

  float t = find_quad_max_curvature(quad);
  if (t == 0 || t == 1) {
    return kLine_ReductionType;
  }
  QuadCoeff::EvalQuadAt(std::array<Point, 3>{quad[0], quad[1], quad[2]}, t,
                        reduction, nullptr);
  return kDegenerate_ReductionType;
}

PathStroker::PathStroker(const Path& src, float radius, float miter_limit,
                         Paint::Cap cap, Paint::Join join, float scale)
    : radius_(radius), scale_(scale), miter_limit_(miter_limit) {
  fSegmentCount = -1;
  fFirstOuterPtIndexInContour = 0;

  fInvResScale = 1.f / (scale * 4);
  fInvResScaleSquared = fInvResScale * fInvResScale;
  fRecursionDepth = 0;

  if (join == Paint::kMiter_Join && miter_limit <= 1.f) {
    join = Paint::kBevel_Join;
  }

  cap_proc_ = CapFactory(cap);
  join_proc_ = JoinFactory(join);
}

/// Stroke
Stroke::Stroke(const Paint& paint)
    : width_(paint.GetStrokeWidth()),
      miter_limit_(paint.GetStrokeMiter()),
      cap_(paint.GetStrokeCap()),
      join_(paint.GetStrokeJoin()) {}

void Stroke::StrokePath(const Path& src, Path* dst) const {
  PathStroker stroker(src, width_ * 0.5f, miter_limit_, cap_, join_, 1.f);

  Path::Iter iter{src, false};
  std::array<Point, 4> pts = {};
  Path::Verb last_verb = Path::Verb::kMove;
  for (;;) {
    Path::Verb verb = iter.Next(pts.data());

    switch (verb) {
      case Path::Verb::kMove:
        stroker.MoveTo(pts[0]);
        break;
      case Path::Verb::kLine:
        stroker.LineTo(pts[1]);
        last_verb = Path::Verb::kLine;
        break;
      case Path::Verb::kQuad:
        stroker.QuadTo(pts[1], pts[2]);
        last_verb = Path::Verb::kQuad;
        break;
      case Path::Verb::kConic:
        break;
      case Path::Verb::kCubic:
        break;
      case Path::Verb::kClose:
        stroker.Close(last_verb == Path::Verb::kLine);
        break;
      case Path::Verb::kDone:
        goto DONE;
        break;
    }
  }
DONE:
  stroker.Done(dst, last_verb == Path::Verb::kLine);
}

[[maybe_unused]] static void mark_point(Path* dst, Point point) {
  dst->LineTo(point.x + 50, point.y);
  dst->LineTo(point.x, point.y);
  dst->LineTo(point.x, point.y + 50);
  dst->LineTo(point.x, point.y);
}

void Stroke::QuadPath(const Path& src, Path* dst) const {
  Path::Iter iter{src, false};
  std::array<Point, 4> pts = {};
  for (;;) {
    Path::Verb verb = iter.Next(pts.data());
    switch (verb) {
      case Path::Verb::kMove:
        dst->MoveTo(pts[0]);
        break;
      case Path::Verb::kLine:
        dst->LineTo(pts[1]);
        break;
      case Path::Verb::kQuad:
        dst->QuadTo(pts[1], pts[2]);
        break;
      case Path::Verb::kConic: {
        Conic conic(pts[0], pts[1], pts[2], iter.ConicWeight());
        constexpr auto kPow2 = 1;  // Only works for sweeps up to 90 degrees.
        constexpr auto kQuadCount = 1 + (2 * (1 << kPow2));  // 5
        Point points[kQuadCount];
        conic.ChopIntoQuadsPOW2(points, kPow2);
        dst->QuadTo(points[1], points[2]);
        // mark_point(dst, points[2]);
        dst->QuadTo(points[3], points[4]);
        // mark_point(dst, points[4]);
      } break;
      case Path::Verb::kCubic: {
        Cubic cubic(pts[0], pts[1], pts[2], pts[3]);
        std::vector<std::array<Point, 3>> quads = cubic.ToQuads();
        for (auto& points : quads) {
          // mark_point(dst, points[0]);
          dst->QuadTo(points[1], points[2]);
        }
      } break;
      case Path::Verb::kClose:
        dst->Close();
        break;
      case Path::Verb::kDone:
        goto DONE;
        break;
    }
  }
DONE:
  return;
}

}  // namespace skity
