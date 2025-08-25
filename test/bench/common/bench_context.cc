// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "test/bench/common/bench_context.hpp"

#include <memory>

#ifdef SKITY_BENCH_MTL_BACKEND
#include "test/bench/common/bench_context_mtl.h"
#endif

#ifdef SKITY_BENCH_GL_BACKEND
#include "test/bench/common/bench_context_gl.hpp"
#include "test/bench/common/bench_gl_context_mac.h"
#endif

namespace skity {

std::shared_ptr<BenchContext> BenchContext::Create(GPUBackendType type) {
#ifdef SKITY_BENCH_MTL_BACKEND
  if (GPUBackendType::kMetal == type) {
    return CreateBenchContextMTL();
  }
#endif

#ifdef SKITY_BENCH_GL_BACKEND
  if (GPUBackendType::kOpenGL == type) {
    return CreateBenchContextGL(GetGLProcLoader());
  }
#endif
  return nullptr;
}
}  // namespace skity
