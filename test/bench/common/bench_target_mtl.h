// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_COMMON_BENCH_TARGET_MTL_H
#define TEST_BENCH_COMMON_BENCH_TARGET_MTL_H

#import <Metal/Metal.h>

#include <cassert>
#include <memory>
#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/gpu/gpu_context.hpp>

#include "test/bench/common/bench_target.hpp"

namespace skity {

class BenchTargetMTL : public BenchTarget {
 public:
  static std::shared_ptr<BenchTarget> Create(skity::GPUContext *context,
                                             Options options);

  BenchTargetMTL(skity::GPUContext *context,
                 std::unique_ptr<skity::GPUSurface> surface, Options options,
                 id<MTLTexture> texture);

  id<MTLTexture> GetTexture() const { return texture_; }

 private:
  id<MTLTexture> texture_;
};

}  // namespace skity

#endif  // TEST_BENCH_COMMON_BENCH_TARGET_MTL_H
