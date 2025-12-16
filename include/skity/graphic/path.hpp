// Copyright 2006 The Android Open Source Project

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_PATH_HPP
#define INCLUDE_SKITY_GRAPHIC_PATH_HPP

#include <array>
#include <cstdint>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/point.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/geometry/rrect.hpp>
#include <skity/macros.hpp>
#include <tuple>
#include <vector>

namespace skity {

class SKITY_API Path {
 public:
  enum class AddMode {
    // append to destination unaltered
    kAppend,
    // add line if prior contour is not closed
    kExtend,
  };

  enum class ConvexityType {
    kUnknown,
    kConvex,
    kConcave,
  };

  enum class Direction {
    // clockwise direction for adding closed contours
    kCW,
    // counter-clockwise direction for adding closed contours
    kCCW,
    kUnknown,
  };

  enum class Verb {
    // Path move command, iter.next returns 1 point
    kMove,
    // Path LineTo command, iter.next returns 2 points
    kLine,
    // Path QuadTo command, iter.next retruns 3 points
    kQuad,
    // Path ConicTo command, iter.next return 3 points + iter.conicWeight()
    kConic,
    // Path CubicTo command, iter.next return 4 points
    kCubic,
    // Path close command, iter.next return 1 points
    kClose,
    // iter.next return 0 points
    kDone,
  };

  enum class PathFillType {
    // "inside" is computed by a non-zero sum of signed edge crossings
    kWinding,
    // "inside" is computed by an odd number of edge crossings
    kEvenOdd,
  };

  class Iter {
   public:
    /**
     * @brief Create empty Path::Iter
     *        Call setPath to initialize Path::iter at a later time.
     */
    Iter();
    /**
     * @brief             Create Path::Iter with given path object. And
     *                    indicate whether force to close this path.
     *
     * @param path        path tobe iterated
     * @param forceClose  insert close command if need
     */
    Iter(Path const& path, bool forceClose);

    ~Iter() = default;

    void SetPath(Path const& path, bool forceClose);

    /**
     * @brief         Returns next Path::Verb in verb array, and advances
     *                Path::Iter
     *
     * @param pts     storage for Point data describing returned Path::Verb
     * @return Verb   next Path::Verb from verb array
     */
    Verb Next(Point pts[4]);

    /**
     * @brief         Retruns conic weight if next() returned Verb::kConic
     *
     * @return float  conic weight for conic Point returned by next()
     */
    float ConicWeight() const;

    /**
     * @brief         Returns true if last kLine returned by next() was
     *                genearted by kClose.
     *
     * @return true   last kLine was gererated by kClose.
     * @return false  otherwise
     */
    bool IsCloseLine() const;

    bool IsClosedContour() const;

   private:
    Verb AutoClose(Point pts[2]);
    Point const& ConsMoveTo();

   private:
    const Point* pts_;
    const Verb* verbs_;
    const Verb* verb_stop_;
    const float* conic_weights_;
    bool force_close_;
    bool need_close_;
    bool close_line_;
    Point move_to_;
    Point last_pt_;
    enum class SegmentState {
      /**
       * @brief The current contour is empty. Starting processing or have just
       * closed a contour.
       */
      kEmptyContour,
      /**
       * @brief Have seen a move, but nothing else
       */
      kAfterMove,
      /**
       * @brief Have seen a primitive but not yet closed the path. Also the
       * initial state.
       */
      kAfterPrimitive,
    };
    SegmentState segment_state_;
  };

  class RawIter {
   public:
    RawIter();
    explicit RawIter(Path const& path) : RawIter() { SetPath(path); }
    ~RawIter() = default;

    void SetPath(Path const& path);

    Verb Next(Point pts[4]);
    Verb Peek() const;
    float ConicWeight() const;

   private:
    const Point* pts_;
    const Verb* verbs_;
    const Verb* verb_stop_;
    const float* conic_weights_;
  };

  class RangeIter final {
   public:
    RangeIter() = default;
    RangeIter(const Verb* verbs, const Point* points, const float* weights)
        : verb_(verbs), points_(points), weights_(weights) {}

    bool operator!=(RangeIter const& other) const {
      return verb_ != other.verb_;
    }

    bool operator==(RangeIter const& other) const {
      return verb_ == other.verb_;
    }

    RangeIter& operator++() {
      auto verb = *verb_++;
      points_ += pts_advance_after_verb(verb);
      if (verb == Verb::kConic) {
        ++weights_;
      }
      return *this;
    }

    RangeIter operator++(int) {
      RangeIter copy = *this;
      this->operator++();
      return copy;
    }

    std::tuple<Verb, const Point*, const float*> operator*() const {
      Verb verb = this->PeekVerb();
      int backset = pts_backset_for_verb(verb);
      return {verb, points_ + backset, weights_};
    }

    Verb PeekVerb() const { return *verb_; }

   private:
    constexpr static int pts_advance_after_verb(Verb verb) {
      switch (verb) {
        case Verb::kMove:
          return 1;
        case Verb::kLine:
          return 1;
        case Verb::kQuad:
          return 2;
        case Verb::kConic:
          return 2;
        case Verb::kCubic:
          return 3;
        case Verb::kClose:
          return 0;
        default:
          break;
      }
      // can not reach here
      return 0;
    }

    constexpr static int pts_backset_for_verb(Verb verb) {
      switch (verb) {
        case Verb::kMove:
          return 0;
        case Verb::kLine:
          return -1;
        case Verb::kQuad:
          return -1;
        case Verb::kConic:
          return -1;
        case Verb::kCubic:
          return -1;
        case Verb::kClose:
          return -1;
        default:
          break;
      }
      // can not reach here
      return 0;
    }

   private:
    const Verb* verb_ = nullptr;
    const Point* points_ = nullptr;
    const float* weights_ = nullptr;
  };

  Path() {
    points_.reserve(4);
    verbs_.reserve(4);
    conic_weights_.reserve(2);
  }
  ~Path() = default;

  Path(Path const&) = default;
  Path& operator=(Path const&) = default;
  Path(Path&&) = default;

  inline size_t CountPoints() const { return points_.size(); }
  inline size_t CountVerbs() const { return verbs_.size(); }

  Path& MoveTo(float x, float y);
  Path& MoveTo(Point const& point) { return MoveTo(point.x, point.y); }
  Path& LineTo(Point const& point) { return LineTo(point.x, point.y); }
  Path& LineTo(float x, float y);
  Path& QuadTo(float x1, float y1, float x2, float y2);
  Path& QuadTo(Point const& p1, Point const& p2) {
    return this->QuadTo(p1.x, p1.y, p2.x, p2.y);
  }
  Path& ConicTo(float x1, float y1, float x2, float y2, float weight);
  Path& ConicTo(Point const& p1, Point const& p2, float weight) {
    return this->ConicTo(p1.x, p1.y, p2.x, p2.y, weight);
  }
  Path& CubicTo(float x1, float y1, float x2, float y2, float x3, float y3);
  Path& CubicTo(Point const& p1, Point const& p2, Point const& p3) {
    return this->CubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
  }
  Path& ArcTo(float x1, float y1, float x2, float y2, float radius);

  /**
   * @brief Appends arc to Path.
   *        arc added is part of ellipse bounded by oval, from startAngle
   *        through sweepAngle. Both startAngle and sweepAngle are measured in
   *        degrees, where zero degrees is aligned with the positive x-axis, and
   *        positive sweeps extends arc clockwise.
   *
   * @param oval        bounds of ellipse containing arc
   * @param startAngle  starting angle of arc in degrees
   * @param sweepAngle  in degrees. Positive is clockwise; treated modulo 360
   * @param forceMove   true to start a new contour with arc
   * @return Path&      reference to self.
   */
  Path& ArcTo(Rect const& oval, float startAngle, float sweepAngle,
              bool forceMove);

  enum ArcSize {
    kSmall,
    kLarge,
  };

  /**
   *
   * @param rx              radius on x-axis
   * @param ry              radius on y-axis
   * @param xAxisRotate     x-axis rotation in degrees; positive values are
   *                        clockwise
   * @param largeArc        choose smaller or larger arc
   * @param sweep           choose clockwise or counterclockwise arc
   * @param x               end of arc
   * @param y               end of arc
   * @return                reference to Path
   */
  Path& ArcTo(float rx, float ry, float xAxisRotate, ArcSize largeArc,
              Direction sweep, float x, float y);
  Path& Close();
  Path& Reset();
  Path& ReverseAddPath(const Path& src);

  /**
   * Adds circle centered at (x, y) of size radius to Path, appending kMove,
   * four kConic, and kClose. Circle begins at: (x + radius, y), continuing
   * clockwise if [dir] is kCW direction, and counterclockwise if [dir] is kCCW.
   *
   * @note Has no effect if radius is zero or negative.
   * @param x           center of circle
   * @param y           center of circle
   * @param radius      distance from center to edge
   * @param dir         Path::PathDirection to wind circle
   * @return            reference to Path self
   */
  Path& AddCircle(float x, float y, float radius,
                  Direction dir = Direction::kCW);

  /**
   * Adds oval to path, appending kMove, four kConic, and kClose.
   * Oval is upright ellipse bounded by Rect oval with radii equal to half oval
   * width and half oval height.
   *
   * @param oval    bounds of ellipse added
   * @param dir     Path::Direction to wind ellipse
   * @return        reference to Path self
   */
  Path& AddOval(const Rect& oval, Direction dir = Direction::kCW);

  /**
   * Adds oval to path, appending kMove, four kConic, and kClose.
   *
   * @param oval    bounds of ellipse added
   * @param dir     Path::Direction to wind ellipse
   * @param start   index of initial point of ellipse
   * @return        reference to Path self
   */
  Path& AddOval(const Rect& oval, Direction dir, uint32_t start);

  /**
   * Adds a new contour to the path, defined by the rect, and wound in the
   * specified direction. The verbs added to the path will be :
   *  kMove, kLine, kLine, kLine, kClose
   *
   * start specifies which corner to begin the contour:
   *  0: upper-left corner
   *  1: upper-right corner
   *  2: lower-right corner
   *  3: lower-left corner
   *
   * @param rect    Rect to add as a closed contour
   * @param dir     Path::Direction to orient the new contour
   * @param start   initial corner of Rect to add
   * @return Path&  reference to Path
   */
  Path& AddRect(Rect const& rect, Direction dir, uint32_t start);

  Path& AddRect(Rect const& rect, Direction dir = Direction::kCW) {
    return this->AddRect(rect, dir, 0);
  }

  Path& AddRect(float left, float top, float right, float bottom,
                Direction dir = Direction::kCW) {
    return this->AddRect({left, top, right, bottom}, dir, 0);
  }

  Path& AddRoundRect(Rect const& rect, float rx, float ry,
                     Direction dir = Direction::kCW);

  Path& AddRoundRect(Rect const& rect, const float radii[],
                     Direction dir = Direction::kCW);

  Path& AddRRect(RRect const& rrect, Direction dir = Direction::kCW);

  Path& AddRRect(RRect const& rrect, Direction dir, uint32_t start);
  /**
   * Append, in reverse order, the first contour of path, ignoring path's last
   * point. If no MoveTo() call has been made for this contour, the first point
   * is automatically to (0, 0)
   *
   */
  Path& ReversePathTo(const Path& src);
  bool GetLastPt(Point* lastPt) const;
  Point GetPoint(int index) const;
  Path::Verb GetVerb(int index) const;
  Point GetLastMovePt() const;
  /**
   * Returns true for finite Point array values between negative float and
   * positive float. Returns false for any Point array value of FloatInfinity
   * value or FloatNaN.
   *
   * @return true if all Point values are finite
   */
  bool IsFinite() const;
  /**
   * Returns true if Path contains only one line;
   * @param line    if Path contains one line and line is not nullptr, line is
   *                set to this array
   * @return        true if Path contains exactly one line
   */
  bool IsLine(Point line[2]) const;
  bool IsEmpty() const { return 0 == CountVerbs(); }

  bool operator==(const Path& other) const;
  bool operator!=(const Path& other) const { return !(*this == other); }
  void Swap(Path& that);

  /**
   * Appends src to Path, offset by (dx, dy)
   *
   *    If mode is kAppend, src verb array, point array, and conic weights are
   * added unaltered. If mode is kExtend, add line before appending verbs,
   * point, and conic weights.
   *
   */
  Path& AddPath(const Path& src, float dx, float dy,
                AddMode mode = AddMode::kAppend);
  Path& AddPath(const Path& src, AddMode mode = AddMode::kAppend);
  Path& AddPath(const Path& src, const Matrix& matrix,
                AddMode mode = AddMode::kAppend);
  /**
   * Sets last point to (x, y).
   * If Point array is empty, append kMove to verb array.
   * @param x   set x-axis value of last point
   * @param y   set y-axis value of last point
   */
  void SetLastPt(float x, float y);
  void SetLastPt(const Point& p) { this->SetLastPt(p.x, p.y); }

  Direction GetFirstDirection() const { return first_direction_; }
  inline void SetFirstDirection(Direction dir) { this->first_direction_ = dir; }

  /**
   * @brief Sets FillType, the rule used to fill Path.
   *        Default value is PathFillType::kWinding
   *
   * @param type FillType
   */
  void SetFillType(PathFillType type) { fill_type_ = type; }

  /**
   * @brief Returns PathFillType, the rule used to fill Path.
   *
   * @return PathFillType
   */
  PathFillType GetFillType() const { return fill_type_; }

  /**
   * Returns true if Path is equivalent to Rect when filled.
   * If false: rect, is_closed and direction are unchanged.
   * If true: rect, is_closed and direction are written to if not nullptr.
   *
   * @param rect        storage for bounds of Rect; may be nullptr
   * @param is_closed   storage set to true if Path is closed; may be nullptr.
   * @param direction   storage set to Rect direction; may be nullptr.
   * @return true       Path is equivalent to rect
   */
  bool IsRect(Rect* rect, bool* is_closed = nullptr,
              Direction* direction = nullptr) const;

  /**
   * Returns true if point(x, y) is contained by Path. This taking PathFillType
   * into account.
   *
   * @param x       x-axis value of containment test
   * @param y       y-axis value of containment test
   * @return        true if Path contains this point(x, y)
   */
  bool Contains(float x, float y) const;

  Rect GetBounds() const {
    ComputeBounds();
    return bounds_;
  }
  /**
   * dump Path content into std::out
   */
  void Dump();

  const Verb* VerbsBegin() const { return verbs_.data(); }
  const Verb* VerbsEnd() const { return verbs_.data() + CountVerbs(); }
  const Point* Points() const { return points_.data(); }
  const float* ConicWeights() const { return conic_weights_.data(); }

  /**
   * @internal
   * @param matrix
   * @return
   */
  Path CopyWithMatrix(Matrix const& matrix) const;
  Path CopyWithScale(float scale) const;

  void SetConvexityType(ConvexityType type) { convexity_ = type; }
  ConvexityType GetConvexityType() const {
    if (convexity_ == ConvexityType::kUnknown) {
      convexity_ = ComputeConvexity();
    }
    return convexity_;
  }
  bool IsConvex() const { return GetConvexityType() == ConvexityType::kConvex; }

  struct SegmentMask {
    enum Value : uint32_t {
      kLine = 0x0001,
      kQuad = 0x0002,
      kConic = 0x0004,
      kCubic = 0x0008,
    };
  };

  /**
   * Returns a mask, where each set bit corresponds to a SegmentMask constant if
   * Path contains one or more verbs of that type. Returns zero if Path contains
   * no lines, or curves: quads, conics, or cubics.
   *
   * GetSegmentMasks() returns a cached result; it is very fast.
   * @return  SegmentMask bits or zero
   */
  uint32_t GetSegmentMasks() const { return segment_masks_; }

 private:
  void InjectMoveToIfNeed();
  void ComputeBounds() const;
  Path::ConvexityType ComputeConvexity() const;
  int LeadingMoveToCount() const;
  inline const Point& AtPoint(int32_t index) const { return points_[index]; }
  bool HasOnlyMoveTos() const;

  bool IsZeroLengthSincePoint(int startPtIndex) const;
  static bool ComputePtBounds(Rect* bounds, Path const& ref);

 private:
  friend class Iter;
  friend class RawIter;
  friend class PathStroker;

  int32_t last_move_to_index_ = ~0;
  mutable ConvexityType convexity_ = ConvexityType::kUnknown;
  mutable Direction first_direction_ = Direction::kCCW;

  std::vector<Point> points_;
  std::vector<Verb> verbs_;
  std::vector<float> conic_weights_;
  mutable bool is_finite_ = true;
  mutable Rect bounds_;
  PathFillType fill_type_ = PathFillType::kWinding;
  uint32_t segment_masks_ = 0;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_PATH_HPP
