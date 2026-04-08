// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/golden_test_env.hpp"

#ifdef SKITY_GOLDEN_GUI

#include <GLFW/glfw3.h>

#endif

namespace skity {
namespace testing {

GoldenTestEnv* CreateGoldenTestEnvMTL();
GoldenTestEnv* CreateGoldenTestEnvGL();

GoldenTestEnv* g_golden_test_env = nullptr;

GoldenTestEnv* GoldenTestEnv::CreateInstance(Backend backend) {
  switch (backend) {
    case Backend::kGL:
      g_golden_test_env = CreateGoldenTestEnvGL();
      break;
    case Backend::kVulkan:
      break;
    case Backend::kMetal:
      g_golden_test_env = CreateGoldenTestEnvMTL();
      break;
    default:
      return nullptr;
  }

  return g_golden_test_env;
}

GoldenTestEnv* GoldenTestEnv::GetInstance() { return g_golden_test_env; }

void GoldenTestEnv::SetUp() {
#ifdef SKITY_GOLDEN_GUI
  // Init glfw for all testing case
  glfwInit();
  // Metal does not needs GL client api
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

  gpu_context_ = CreateGPUContext();
}

void GoldenTestEnv::TearDown() {
  gpu_context_.reset();

#ifdef SKITY_GOLDEN_GUI

  glfwTerminate();

#endif
}

}  // namespace testing
}  // namespace skity
