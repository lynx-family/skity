// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_COMMON_BENCH_GPU_TIME_TRACER_HPP
#define TEST_BENCH_COMMON_BENCH_GPU_TIME_TRACER_HPP

#include <cassert>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>

namespace skity {

struct GPUTimeDuration {
  double start = std::numeric_limits<double>::max();
  double end = 0.0;
};

struct GPUFrame {
  int32_t index;
  int32_t command_buffer_count = 0;
  std::vector<GPUTimeDuration> durations;
};

class BenchGPUTimeTracer {
 public:
  static BenchGPUTimeTracer& Instance() {
    static BenchGPUTimeTracer instance;
    return instance;
  }

  void StartFrame();

  void EndFrame();

  void StartTracing() {
    if (!is_enable_) {
      return;
    }
    is_tracing_ = true;
    start_tracing_();
  }

  void StopTracing() {
    if (!is_enable_) {
      return;
    }
    stop_tracing_();
    is_tracing_ = false;
  }

  bool IsTracing() const { return is_tracing_; }

  int32_t CurrentFrameIndex() const {
    assert(is_enable_);
    assert(frames_.size() > 0);
    return frames_.size() - 1;
  }

  void AppendGPUFrameTime(int32_t frame_index, double start, double end) {
    assert(is_enable_);
    assert(frame_index >= 0);
    assert(frame_index < frames_.size());
    GPUTimeDuration duration;
    duration.start = start;
    duration.end = end;
    frames_[frame_index].durations.push_back(duration);
  }

  void SetCallback(std::function<void()> start_tracing,
                   std::function<void()> stop_tracing) {
    start_tracing_ = start_tracing;
    stop_tracing_ = stop_tracing;
  }

  uint32_t GetFrameSize() {
    assert(is_enable_);
    return frames_.size();
  }

  int32_t GetAverageFrameTime() {
    assert(is_enable_);
    double sum = 0;
    assert(frames_.size() > 0);
    for (auto frame : frames_) {
      for (auto duration : frame.durations) {
        sum = sum + duration.end - duration.start;
      }
    }
    return std::round(sum / frames_.size() * 1000000);
  }

  void SetEnable(bool enable) { is_enable_ = enable; }

  void ClearFrame() { frames_.clear(); }

  bool IsEnable() const { return is_enable_; }

 private:
  std::vector<GPUFrame> frames_;
  bool is_tracing_ = false;
  std::function<void()> start_tracing_;
  std::function<void()> stop_tracing_;
  bool is_enable_ = false;
};

}  // namespace skity

#endif  // TEST_BENCH_COMMON_BENCH_GPU_TIME_TRACER_HPP
