// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SKITY_EXAMPLE_COMMON_VK_WINDOW_VK_HPP
#define SKITY_EXAMPLE_COMMON_VK_WINDOW_VK_HPP

#include <volk.h>

#include <memory>

#include "common/window.hpp"

namespace skity {
class VkInterface;

namespace example {

class WindowVK : public Window {
 public:
  WindowVK(int width, int height, std::string title);
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
  VkSurfaceKHR vk_surface_ = VK_NULL_HANDLE;
  std::unique_ptr<skity::GPUSurface> window_surface_;
};

}  // namespace example
}  // namespace skity

#endif  // SKITY_EXAMPLE_COMMON_VK_WINDOW_VK_HPP