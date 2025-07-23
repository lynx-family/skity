/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_PATH_PRIV_HPP
#define SRC_GRAPHIC_PATH_PRIV_HPP

#include <skity/graphic/path.hpp>

namespace skity {

class PathPriv {
 public:
  struct Iterate {
    explicit Iterate(Path const& path)
        : Iterate(path.VerbsBegin(),
                  (!path.IsFinite()) ? path.VerbsBegin() : path.VerbsEnd(),
                  path.Points(), path.ConicWeights()) {}

    Iterate(const Path::Verb* verbs_begin, const Path::Verb* verbs_end,
            const Point* points, const float* weights)
        : verbs_begin_(verbs_begin),
          verbs_end_(verbs_end),
          points_(points),
          weights_(weights) {}

    Path::RangeIter begin() {
      return Path::RangeIter{verbs_begin_, points_, weights_};
    }

    Path::RangeIter end() {
      return Path::RangeIter{verbs_end_, nullptr, nullptr};
    }

   private:
    const Path::Verb* verbs_begin_;
    const Path::Verb* verbs_end_;
    const Point* points_;
    const float* weights_;
  };

  static void CreateDrawArcPath(Path* path, Rect const& oval, float startAngle,
                                float sweepAngle, bool useCenter,
                                bool isFillNoPathEffect);

  /**
   * Determines if path is a rect by keeping track of changes in direction
   * and looking for a loop either clockwise or counterclockwise.
   *
   * The direction is computed such that:
   *  0: vertical up
   *  1: horizontal left
   *  2: vertical down
   *  3: horizontal right
   *
   * @param dx
   * @param dy
   * @return int32_t
   */
  static int32_t RectMakeDir(float dx, float dy);
};

// Lightweight variant of SkPath::Iter that only returns segments (e.g.
// lines/conics). Does not return kMove or kClose. Always "auto-closes" each
// contour. Roughly the same as SkPath::Iter(path, true), but does not return
// moves or closes
//
class PathEdgeIter {
  const Path::Verb* verbs_;
  const Path::Verb* verbs_stop_;
  const Point* points_;
  const Point* move_to_ptr_;
  const float* conic_weights_;
  Point scratch_[2];  // for auto-close lines
  bool needs_close_line_;
  bool next_is_new_contour_;

  enum { kIllegalEdgeValue = 99 };

 public:
  explicit PathEdgeIter(const Path& path) {
    move_to_ptr_ = points_ = path.Points();
    verbs_ = path.VerbsBegin();
    verbs_stop_ = path.VerbsEnd();
    conic_weights_ = path.ConicWeights();
    if (conic_weights_) {
      conic_weights_ -= 1;  // begin one behind
    }

    needs_close_line_ = false;
    next_is_new_contour_ = false;
  }

  float conicWeight() const { return *conic_weights_; }

  enum class Edge {
    kLine = static_cast<int>(Path::Verb::kLine),
    kQuad = static_cast<int>(Path::Verb::kQuad),
    kConic = static_cast<int>(Path::Verb::kConic),
    kCubic = static_cast<int>(Path::Verb::kCubic),
  };

  static Path::Verb EdgeToVerb(Edge e) { return Path::Verb(e); }

  struct Result {
    const Point* points;  // points for the segment, or null if done
    Edge edge;
    bool is_new_contour;

    // Returns true when it holds an Edge, false when the path is done.
    explicit operator bool() { return points != nullptr; }
  };

  Result next() {
    auto closeline = [&]() {
      scratch_[0] = points_[-1];
      scratch_[1] = *move_to_ptr_;
      needs_close_line_ = false;
      next_is_new_contour_ = true;
      return Result{scratch_, Edge::kLine, false};
    };

    for (;;) {
      if (verbs_ == verbs_stop_) {
        return needs_close_line_
                   ? closeline()
                   : Result{nullptr, Edge(kIllegalEdgeValue), false};
      }

      const auto v = *verbs_++;
      switch (Path::Verb(v)) {
        case Path::Verb::kMove: {
          if (needs_close_line_) {
            auto res = closeline();
            move_to_ptr_ = points_++;
            return res;
          }
          move_to_ptr_ = points_++;
          next_is_new_contour_ = true;
        } break;
        case Path::Verb::kClose:
          if (needs_close_line_) return closeline();
          break;
        default: {
          // Actual edge.
          const int pts_count = (static_cast<int>(v) + 2) / 2,
                    cws_count = v == Path::Verb::kConic ? 1 : 0;

          needs_close_line_ = true;
          points_ += pts_count;
          conic_weights_ += cws_count;

          bool isNewContour = next_is_new_contour_;
          next_is_new_contour_ = false;
          return {&points_[-(pts_count + 1)], Edge(v), isNewContour};
        }
      }
    }
  }
};

}  // namespace skity

#endif  // SRC_GRAPHIC_PATH_PRIV_HPP
