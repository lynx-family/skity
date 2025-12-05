// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_CASE_DRAW_SKP_HPP
#define TEST_BENCH_CASE_DRAW_SKP_HPP

#include <skity/io/picture.hpp>
#include <skity/io/stream.hpp>
#include <skity/skity.hpp>

#include "test/bench/case/benchmark.hpp"

namespace skity {

class DrawSKPBenchmark : public Benchmark {
 public:
  DrawSKPBenchmark(std::string name, const char* skp_file_path, uint32_t width,
                   uint32_t height, Matrix matrix = Matrix{});

  Size GetSize() override { return {width_, height_}; }

  std::string GetName() override { return name_; }

  void OnDraw(Canvas* canvas, int index) override;

 private:
  std::string name_;
  std::unique_ptr<DisplayList> display_list_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
};
}  // namespace skity

#endif  // TEST_BENCH_CASE_DRAW_SKP_HPP
