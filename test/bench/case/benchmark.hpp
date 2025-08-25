// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_CASE_BENCHMARK_HPP
#define TEST_BENCH_CASE_BENCHMARK_HPP

#include <skity/skity.hpp>

namespace skity {

class Benchmark {
 public:
  struct Size {
    uint32_t width;
    uint32_t height;
  };

  void Draw(Canvas *canvas, int index) {
    canvas->Save();
    OnDraw(canvas, index);
    canvas->Restore();
  }

  virtual Size GetSize() = 0;
  virtual std::string GetName() = 0;

 protected:
  virtual void OnDraw(Canvas *canvas, int index) = 0;
};
}  // namespace skity

#endif  // TEST_BENCH_CASE_BENCHMARK_HPP
