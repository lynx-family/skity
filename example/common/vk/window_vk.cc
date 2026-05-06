// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// This example uses volk as the Vulkan loader. Keep `VK_NO_PROTOTYPES`
// defined before including any Vulkan headers so the TU consistently uses
// volk-managed function pointers instead of mixing prototype declarations with
// dynamic loader symbols.
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "common/vk/window_vk.hpp"

#include <volk.h>

#if defined(SKITY_WIN)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(SKITY_MACOS)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(SKITY_LINUX)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace skity {
namespace example {

#if defined(SKITY_MACOS)
bool SetupCocoaVulkanWindow(GLFWwindow* window);
void* GetCocoaVulkanLayer(GLFWwindow* window);
void* GetCocoaVulkanView(GLFWwindow* window);
#endif

namespace {

void AddUniqueExtension(std::vector<const char*>* extensions,
                        const char* name) {
  if (extensions == nullptr || name == nullptr) {
    return;
  }

  if (std::find(extensions->begin(), extensions->end(), name) !=
      extensions->end()) {
    return;
  }

  extensions->push_back(name);
}

bool ContainsString(const char* const* names, uint32_t count,
                    const char* target) {
  if (names == nullptr || target == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < count; ++i) {
    if (names[i] != nullptr && std::string(names[i]) == target) {
      return true;
    }
  }

  return false;
}

bool BuildNativeWindowInfo(GLFWwindow* window,
                           skity::VKNativeWindowInfo* info) {
  if (window == nullptr || info == nullptr) {
    return false;
  }

  *info = {};

#if defined(SKITY_WIN)
  info->type = skity::VKNativeWindowType::kWin32;
  info->handle = glfwGetWin32Window(window);
  info->secondary_handle = GetModuleHandle(nullptr);
  return info->handle != nullptr;
#elif defined(SKITY_MACOS)
  info->type = skity::VKNativeWindowType::kMetalLayer;
  info->handle = GetCocoaVulkanLayer(window);
  info->secondary_handle = GetCocoaVulkanView(window);
  return info->handle != nullptr || info->secondary_handle != nullptr;
#elif defined(SKITY_LINUX)
  uint32_t extension_count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&extension_count);
  if (ContainsString(extensions, extension_count,
                     VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME)) {
    info->type = skity::VKNativeWindowType::kWayland;
    info->handle = glfwGetWaylandDisplay();
    info->secondary_handle = glfwGetWaylandWindow(window);
    return info->handle != nullptr && info->secondary_handle != nullptr;
  }

  info->type = skity::VKNativeWindowType::kXlib;
  info->handle = glfwGetX11Display();
  info->window_id = static_cast<uint64_t>(glfwGetX11Window(window));
  return info->handle != nullptr && info->window_id != 0;
#else
  (void)window;
  return false;
#endif
}

}  // namespace

bool WindowVK::OnInit() {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  return true;
}

GLFWwindow* WindowVK::CreateWindowHandler() {
  auto* window = glfwCreateWindow(GetWidth(), GetHeight(), GetTitle().c_str(),
                                  nullptr, nullptr);
  if (window == nullptr) {
    return nullptr;
  }

#if defined(SKITY_MACOS)
  if (!SetupCocoaVulkanWindow(window)) {
    glfwDestroyWindow(window);
    return nullptr;
  }
#endif

  return window;
}

std::unique_ptr<skity::GPUContext> WindowVK::CreateGPUContext() {
  if (volkInitialize() != VK_SUCCESS) {
    std::cerr << "Failed to initialize volk." << std::endl;
    return nullptr;
  }

  uint32_t required_extension_count = 0;
  const char** required_extensions =
      glfwGetRequiredInstanceExtensions(&required_extension_count);
  if (required_extensions == nullptr || required_extension_count == 0) {
    std::cerr << "Failed to query GLFW Vulkan instance extensions."
              << std::endl;
    return nullptr;
  }

  std::vector<const char*> requested_extensions(
      required_extensions, required_extensions + required_extension_count);

#if defined(SKITY_MACOS) || defined(SKITY_IOS)
#if defined(VK_EXT_METAL_SURFACE_EXTENSION_NAME)
  AddUniqueExtension(&requested_extensions,
                     VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_MVK_MACOS_SURFACE_EXTENSION_NAME)
  AddUniqueExtension(&requested_extensions,
                     VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#endif
#endif

  skity::GPUContextInfoVK info = {};
  info.get_instance_proc_addr = vkGetInstanceProcAddr;
  info.enabled_instance_extensions = requested_extensions.data();
  info.enabled_instance_extension_count =
      static_cast<uint32_t>(requested_extensions.size());
  return skity::CreateGPUContextVK(&info);
}

void WindowVK::OnShow() {
  if (!CreateNativeWindow()) {
    std::cerr << "Failed to create Vulkan native window." << std::endl;
    glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
  }
}

skity::Canvas* WindowVK::AquireCanvas() {
  if (native_window_vk_ == nullptr) {
    return nullptr;
  }

  auto* presenter = native_window_vk_->GetPresenter();
  if (presenter == nullptr) {
    return nullptr;
  }

  const int32_t logical_width = std::max(GetWidth(), 1);
  const int32_t logical_height = std::max(GetHeight(), 1);

  int framebuffer_width = 0;
  int framebuffer_height = 0;
  glfwGetFramebufferSize(GetNativeWindow(), &framebuffer_width,
                         &framebuffer_height);
  const float density =
      static_cast<float>(
          std::max(framebuffer_width, 1) * std::max(framebuffer_width, 1) +
          std::max(framebuffer_height, 1) * std::max(framebuffer_height, 1)) /
      static_cast<float>(logical_width * logical_width +
                         logical_height * logical_height);
  skity::GPUSurfaceAcquireDescriptor acquire_desc = {};
  acquire_desc.sample_count = 4;
  acquire_desc.content_scale = std::sqrt(density);

  auto acquire_result = presenter->AcquireNextSurface(acquire_desc);
  if (acquire_result.status == skity::GPUPresenterStatus::kNeedRecreate) {
    if (!ResizeNativeWindow()) {
      glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
      return nullptr;
    }
    acquire_result =
        native_window_vk_->GetPresenter()->AcquireNextSurface(acquire_desc);
  }

  if (acquire_result.status != skity::GPUPresenterStatus::kSuccess ||
      acquire_result.surface == nullptr) {
    return nullptr;
  }

  surface_ = std::move(acquire_result.surface);
  canvas_ = surface_->LockCanvas();
  return canvas_;
}

void WindowVK::OnPresent() {
  if (canvas_ == nullptr || surface_ == nullptr ||
      native_window_vk_ == nullptr) {
    return;
  }

  canvas_->Flush();
  surface_->Flush();
  canvas_ = nullptr;

  const auto present_status =
      native_window_vk_->GetPresenter()->Present(std::move(surface_));
  if (present_status == skity::GPUPresenterStatus::kNeedRecreate &&
      !ResizeNativeWindow()) {
    glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
  }
}

void WindowVK::OnTerminate() {
  canvas_ = nullptr;
  surface_.reset();
  native_window_vk_.reset();
  ResetGPUContext();
}

bool WindowVK::CreateNativeWindow() {
  int framebuffer_width = 0;
  int framebuffer_height = 0;
  glfwGetFramebufferSize(GetNativeWindow(), &framebuffer_width,
                         &framebuffer_height);

  skity::GPUNativeWindowInfoVK info = {};
  if (!BuildNativeWindowInfo(GetNativeWindow(), &info.native_window)) {
    return false;
  }

  info.width = static_cast<uint32_t>(std::max(framebuffer_width, 1));
  info.height = static_cast<uint32_t>(std::max(framebuffer_height, 1));
  info.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

  native_window_vk_ = skity::CreateGPUNativeWindowVK(GetGPUContext(), &info);
  return native_window_vk_ != nullptr;
}

bool WindowVK::ResizeNativeWindow() {
  if (native_window_vk_ == nullptr) {
    return false;
  }

  int framebuffer_width = 0;
  int framebuffer_height = 0;
  glfwGetFramebufferSize(GetNativeWindow(), &framebuffer_width,
                         &framebuffer_height);

  return native_window_vk_->Resize(
      static_cast<uint32_t>(std::max(framebuffer_width, 1)),
      static_cast<uint32_t>(std::max(framebuffer_height, 1)));
}

}  // namespace example
}  // namespace skity
