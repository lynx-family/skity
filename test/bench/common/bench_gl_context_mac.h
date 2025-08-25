// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_COMMON_GL_CONTEXT_MAC_HPP
#define TEST_BENCH_COMMON_GL_CONTEXT_MAC_HPP

#include <memory>

#include "test/bench/common/bench_gl_context.hpp"

namespace skity {

std::unique_ptr<BenchGLContext> CreateBenchGLContextMac();

void *GetGLProcLoader();

}  // namespace skity

#endif  // TEST_BENCHMARK_COMMON_GL_CONTEXT_MAC_HPP
