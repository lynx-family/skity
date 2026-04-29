// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SKITY_EXAMPLE_COMMON_VK_WINDOW_VK_HPP
#define SKITY_EXAMPLE_COMMON_VK_WINDOW_VK_HPP

#include <volk.h>

#include <memory>
#include <skity/gpu/gpu_context_vk.hpp>
#include <vector>

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
  bool CreateInstance();
  bool CreateSurface();
  bool PickPhysicalDeviceAndQueueFamily();
  bool CreateLogicalDevice();

  std::unique_ptr<skity::GPUSurface> surface_;
  std::unique_ptr<skity::GPUPresenter> presenter_;
  skity::Canvas* canvas_ = nullptr;

  VkInstance instance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_khr_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphics_queue_ = VK_NULL_HANDLE;
  uint32_t graphics_queue_family_index_ = 0;

  std::vector<const char*> enabled_instance_extensions_;
  std::vector<const char*> enabled_instance_layers_;
  std::vector<const char*> enabled_device_extensions_;
};

}  // namespace example
}  // namespace skity

#endif  // SKITY_EXAMPLE_COMMON_VK_WINDOW_VK_HPP
