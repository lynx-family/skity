// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "playground/playground.hpp"

#include <gtest/gtest.h>

#include "playground/window.hpp"

namespace skity {
namespace testing {

bool OpenPlayground(bool passed, std::shared_ptr<GoldenTexture> texture,
                    std::shared_ptr<Pixmap> target, const char* golden_path) {
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();

  std::string title = "[Test] ";
  title.append(test_info->test_case_name());
  title.append(" - ");
  title.append(test_info->name());

  {
    auto window = Window::Create(800, 800, title);

    window->Show(passed, texture, target, golden_path);
  }

  return passed;
}

}  // namespace testing
}  // namespace skity
