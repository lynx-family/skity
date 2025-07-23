// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/golden_test_env.hpp"

namespace skity {
namespace testing {

GoldenTestEnv* CreateGoldenTestEnvMTL();

GoldenTestEnv* g_golden_test_env = nullptr;

GoldenTestEnv* GoldenTestEnv::GetInstance() {
  if (g_golden_test_env == nullptr) {
    g_golden_test_env = CreateGoldenTestEnvMTL();
  }

  return g_golden_test_env;
}

void GoldenTestEnv::SetUp() { gpu_context_ = CreateGPUContext(); }

void GoldenTestEnv::TearDown() { gpu_context_.reset(); }

}  // namespace testing
}  // namespace skity
