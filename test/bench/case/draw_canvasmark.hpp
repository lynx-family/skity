// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_CASE_DRAW_CANVASMARK_HPP
#define TEST_BENCH_CASE_DRAW_CANVASMARK_HPP

#include <memory>

#include "test/bench/case/benchmark.hpp"

namespace skity {
std::shared_ptr<Benchmark> MakeCanvasMarkPathsBenchmark();
std::shared_ptr<Benchmark> MakeCanvasMarkSuitsBenchmark();
}  // namespace skity

#endif  // TEST_BENCH_CASE_DRAW_CANVASMARK_HPP
