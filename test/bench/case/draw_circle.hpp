// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_CASE_DRAW_CIRCLE_HPP
#define TEST_BENCH_CASE_DRAW_CIRCLE_HPP

#include <cassert>
#include <skity/skity.hpp>
#include <sstream>

#include "test/bench/case/benchmark.hpp"

namespace skity {

class DrawCircleBenchmark : public Benchmark {
 public:
  DrawCircleBenchmark(uint32_t count, float radius, bool is_opaque)
      : count_(count), radius_(radius), is_opaque_(is_opaque) {}
  Size GetSize() override { return {1024, 1024}; }
  std::string GetName() override {
    std::stringstream ss;
    ss << "DrawCircle_"
       << "C" << count_ << "_R" << radius_;
    if (is_stroke_) {
      ss << "_SW" << stroke_width_;
    }
    if (is_opaque_) {
      ss << "_O";
    }
    return ss.str();
  }

  void OnDraw(Canvas *canvas, int index) override;

  void SetStrokeWidth(float width) {
    assert(width == 10);
    stroke_width_ = width;
  }

  void SetStroke(bool is_stroke) { is_stroke_ = is_stroke; }

 private:
  uint32_t count_ = 1;
  float radius_;
  bool is_opaque_;
  bool is_stroke_ = false;
  float stroke_width_ = 0;
};
}  // namespace skity

#endif  // TEST_BENCH_CASE_DRAW_CIRCLE_HPP
