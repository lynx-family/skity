// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SKITY_EXAMPLE_COMMON_SW_WINDOW_SW_HPP
#define SKITY_EXAMPLE_COMMON_SW_WINDOW_SW_HPP

#include "common/window.hpp"

namespace skity {
namespace example {

class WindowSW : public Window {
 public:
  WindowSW(int width, int height, std::string title)
      : Window(width, height, std::move(title)) {}
  ~WindowSW() override = default;

  Backend GetBackend() const override { return Backend::kSoftware; }

 protected:
  bool OnInit() override;

  GLFWwindow* CreateWindowHandler() override;

  std::unique_ptr<skity::GPUContext> CreateGPUContext() override;

  void OnShow() override;

  skity::Canvas* AquireCanvas() override;

  void OnPresent() override;

  void OnTerminate() override;

 private:
  std::unique_ptr<skity::Bitmap> bitmap_;
  std::unique_ptr<skity::Canvas> canvas_;

  float screen_scale_ = 1.f;

  uint32_t texture_ = 0;
  uint32_t vbo_ = 0;
  uint32_t vao_ = 0;
  uint32_t index_offset_ = 0;
  uint32_t program_ = 0;
};

}  // namespace example
}  // namespace skity

#endif  // SKITY_EXAMPLE_COMMON_SW_WINDOW_SW_HPP
