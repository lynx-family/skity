// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_COMMON_CONTEXT_GL_HPP
#define TEST_BENCH_COMMON_CONTEXT_GL_HPP

#include "test/bench/common/bench_context.hpp"

namespace skity {

std::shared_ptr<BenchContext> CreateBenchContextGL(void *proc_loader);

}  // namespace skity
#endif  // TEST_BENCH_COMMON_CONTEXT_GL_HPP
