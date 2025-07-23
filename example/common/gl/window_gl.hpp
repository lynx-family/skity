// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SKITY_EXAMPLE_COMMON_GL_WINDOW_GL_HPP
#define SKITY_EXAMPLE_COMMON_GL_WINDOW_GL_HPP

#include "common/window.hpp"

namespace skity {
namespace example {

class WindowGL : public Window {
 public:
  WindowGL(int width, int height, std::string title)
      : Window(width, height, title) {}

  ~WindowGL() override = default;

  Backend GetBackend() const override { return Backend::kOpenGL; }

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

#endif  // SKITY_EXAMPLE_COMMON_GL_WINDOW_GL_HPP
