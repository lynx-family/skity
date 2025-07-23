// Copyright 2006 The Android Open Source Project.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_SW_SW_RASTER_HPP
#define SRC_RENDER_SW_SW_RASTER_HPP

#include <array>
#include <glm/glm.hpp>
#include <limits>
#include <skity/graphic/path.hpp>
#include <vector>

#include "src/render/sw/sw_edge.hpp"
#include "src/render/sw/sw_subpixel.hpp"

namespace skity {

class SpanBuilderDelegate {
 public:
  virtual ~SpanBuilderDelegate() = default;

  virtual void OnBuildSpan(int x, int y, int width, const uint8_t alpha) = 0;
};

class RealSpanBuilder {
 public:
  RealSpanBuilder(const Rect& scan_bounds, SpanBuilderDelegate* delegate)
      : scan_bounds_(scan_bounds), delegate_(delegate) {}

  inline void BuildSpan(int x, int y, const uint8_t alpha);

  inline void BuildSpan(int x, int y, int width, const uint8_t alpha);

  inline void BuildSpans(int x, int y, const uint8_t antialias[], int len);

  std::vector<Span> TakeSpans() { return std::move(spans_); }

 private:
  std::vector<Span> spans_;
  const Rect scan_bounds_;
  SpanBuilderDelegate* delegate_ = nullptr;
};

class SpanBuilder {
 public:
  SpanBuilder(int left, int width, const Rect& scan_bounds,
              SpanBuilderDelegate* span_builder_delegate)
      : left_(left),
        real_span_builder_(scan_bounds, span_builder_delegate),
        scan_bounds_(scan_bounds) {
    alphas_.resize(width);
  }

  void BuildSpan(int x, int y, const uint8_t alpha);

  void BuildSpan(int x, int y, int width, const uint8_t alpha);

  void BuildSpans(int x, int y, const uint8_t antialias[], int len);

  void FlushYIfNeed(int new_y);

  void Flush();

  std::vector<Span> TakeSpans() { return real_span_builder_.TakeSpans(); }

  RealSpanBuilder* GetRealSpanBuilder() { return &real_span_builder_; }

 private:
  std::vector<Alpha> alphas_;
  int curr_y_ = SW_NaN32;
  int left_;
  RealSpanBuilder real_span_builder_;
  const Rect scan_bounds_;
};

void WalkEdges(SWEdge* prev_head, SWEdge* next_tail,
               Path::PathFillType fill_type, SpanBuilder* builder, int start_y,
               int stop_y, SWFixed left_clip, SWFixed right_clip);

class SWRaster {
 public:
  constexpr static Rect kCullRect = Rect::MakeLTRB(-1E9F, -1E9F, 1E9F, 1E9F);
  void SetEvenOdd(bool even_odd) { even_odd_ = even_odd; }
  void RastePath(Path const& path, Matrix const& transform,
                 const Rect& clip_bounds = kCullRect,
                 SpanBuilderDelegate* span_builder_delegate = nullptr);

  std::vector<Span> const& CurrentSpans() const { return spans_; }

  Rect GetBounds() const { return bounds_; }

 private:
  bool even_odd_;
  std::vector<Span> spans_;
  Rect bounds_;
};
}  // namespace skity

#endif  // SRC_RENDER_SW_SW_RASTER_HPP
