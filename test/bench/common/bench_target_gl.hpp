// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_COMMON_BENCH_TARGET_GL_HPP
#define TEST_BENCH_COMMON_BENCH_TARGET_GL_HPP

#include <cassert>
#include <memory>
#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/gpu/gpu_context.hpp>

#include "test/bench/common/bench_target.hpp"

namespace skity {

class BenchTargetGL : public BenchTarget {
 public:
  static std::shared_ptr<BenchTarget> Create(skity::GPUContext *context,
                                             Options options);

  BenchTargetGL(skity::GPUContext *context,
                std::unique_ptr<skity::GPUSurface> surface, Options options,
                uint32_t texture);

  ~BenchTargetGL() override;

  uint32_t GetTexture() const { return texture_; }

 private:
  uint32_t texture_;
};

}  // namespace skity

#endif  // TEST_BENCH_COMMON_BENCH_TARGET_GL_HPP
