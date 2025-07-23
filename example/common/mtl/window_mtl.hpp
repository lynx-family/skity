// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SKITY_EXAMPLE_COMMON_MTL_WINDOW_MTL_HPP
#define SKITY_EXAMPLE_COMMON_MTL_WINDOW_MTL_HPP

#include "common/window.hpp"

namespace skity {
namespace example {

class WindowMTL : public Window {
 public:
  WindowMTL(int width, int height, std::string title)
      : Window(width, height, std::move(title)) {}

  ~WindowMTL() override = default;

  Backend GetBackend() const override { return Backend::kMetal; }

 protected:
  bool OnInit() override;

  GLFWwindow* CreateWindowHandler() override;

  std::unique_ptr<skity::GPUContext> CreateGPUContext() override;

  void OnShow() override;

  skity::Canvas* AquireCanvas() override;

  void OnPresent() override;

  void OnTerminate() override;

 private:
  std::unique_ptr<skity::GPUSurface> surface_;

  skity::Canvas* canvas_ = nullptr;
};

}  // namespace example
}  // namespace skity

#endif  // SKITY_EXAMPLE_COMMON_MTL_WINDOW_MTL_HPP