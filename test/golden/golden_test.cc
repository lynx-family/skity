// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "common/golden_test_env.hpp"

#ifdef SKITY_GOLDEN_GUI
namespace skity {
namespace testing {

::testing::Environment* CreateDiffEnv();

}
}  // namespace skity

#endif

int main(int argc, char* argv[]) {
  /**
   * If --backend="gl/vulkan/metal" is provided, then create the test env for
   * that backend. Otherwise, create the test env for Metal as the default
   * backend.
   */

  skity::testing::Backend backend = skity::testing::Backend::kMetal;

  if (argc > 1) {
    // find the backend in the command line arguments
    for (int i = 1; i < argc; ++i) {
      if (strcmp(argv[i], "--backend") == 0) {
        if (strcmp(argv[i + 1], "gl") == 0) {
          backend = skity::testing::Backend::kGL;
          break;
        } else if (strcmp(argv[i + 1], "vulkan") == 0) {
          backend = skity::testing::Backend::kVulkan;
          break;
        } else if (strcmp(argv[i + 1], "metal") == 0) {
          backend = skity::testing::Backend::kMetal;
          break;
        }
        break;
      }
    }
  }

  testing::InitGoogleTest(&argc, argv);

  testing::AddGlobalTestEnvironment(
      skity::testing::GoldenTestEnv::CreateInstance(backend));

#ifdef SKITY_GOLDEN_GUI
  // GUI needs Metal compute pipeline to generate diff images
  // Create a seperate test env to hold the compute pipeline for GUI
  testing::AddGlobalTestEnvironment(skity::testing::CreateDiffEnv());
#endif

  return RUN_ALL_TESTS();
}
