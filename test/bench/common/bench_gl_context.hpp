// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_COMMON_BENCH_GL_CONTEXT_HPP
#define TEST_BENCH_COMMON_BENCH_GL_CONTEXT_HPP

namespace skity {

class BenchGLContext {
 public:
  BenchGLContext() = default;
  virtual ~BenchGLContext() = default;
  virtual bool MakeCurrent() = 0;
  virtual bool ClearCurrent() = 0;
  virtual bool IsCurrent() = 0;
};

}  // namespace skity

#endif  // TEST_BENCH_COMMON_GL_CONTEXT_HPP
