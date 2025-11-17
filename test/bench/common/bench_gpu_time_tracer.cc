// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "test/bench/common/bench_gpu_time_tracer.hpp"

namespace skity {
void BenchGPUTimeTracer::StartFrame() {
  if (!is_enable_) {
    return;
  }
  GPUFrame frame;
  frame.index = static_cast<int32_t>(frames_.size());
  frames_.push_back(frame);
}

void BenchGPUTimeTracer::EndFrame() {}

}  // namespace skity
