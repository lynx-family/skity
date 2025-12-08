// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_COMMON_BENCH_CONTEXT_HPP
#define TEST_BENCH_COMMON_BENCH_CONTEXT_HPP

#include <memory>
#include <skity/gpu/gpu_context.hpp>

#include "test/bench/common/bench_target.hpp"

namespace skity {

class BenchContext {
 public:
  BenchContext(std::unique_ptr<skity::GPUContext> gpu_context)
      : gpu_context_(std::move(gpu_context)) {}

  static std::shared_ptr<BenchContext> Create(GPUBackendType type);

  virtual ~BenchContext() = default;

  virtual std::shared_ptr<BenchTarget> CreateTarget(
      BenchTarget::Options options) = 0;

  virtual bool WriteToFile(std::shared_ptr<BenchTarget> target,
                           std::string path) = 0;
  GPUContext* GetGPUContext() const { return gpu_context_.get(); }

  virtual void WaitTillFinished() {}

 protected:
  std::unique_ptr<GPUContext> gpu_context_;
};

}  // namespace skity

#endif  // TEST_BENCH_COMMON_BENCH_CONTEXT_HPP
