// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "common/golden_test_env.hpp"

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);

  testing::AddGlobalTestEnvironment(
      skity::testing::GoldenTestEnv::GetInstance());

  return RUN_ALL_TESTS();
}
