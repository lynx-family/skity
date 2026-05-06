// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SKITY_EXAMPLE_COMMON_VK_WINDOW_VK_HPP
#define SKITY_EXAMPLE_COMMON_VK_WINDOW_VK_HPP

#include <memory>
#include <skity/gpu/gpu_context_vk.hpp>

#include "common/window.hpp"

namespace skity {
namespace example {

class WindowVK : public Window {
 public:
  WindowVK(int width, int height, std::string title)
      : Window(width, height, std::move(title)) {}

  ~WindowVK() override = default;

  Backend GetBackend() const override { return Backend::kVulkan; }

 protected:
  bool OnInit() override;

  GLFWwindow* CreateWindowHandler() override;

  std::unique_ptr<skity::GPUContext> CreateGPUContext() override;

  void OnShow() override;

  skity::Canvas* AquireCanvas() override;

  void OnPresent() override;

  void OnTerminate() override;

 private:
  bool CreateNativeWindow();
  bool ResizeNativeWindow();

  std::unique_ptr<skity::GPUSurface> surface_;
  std::unique_ptr<skity::GPUNativeWindowVK> native_window_vk_;
  skity::Canvas* canvas_ = nullptr;
};

}  // namespace example
}  // namespace skity

#endif  // SKITY_EXAMPLE_COMMON_VK_WINDOW_VK_HPP
