// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/vk/window_vk.hpp"

#include <volk.h>  // Include volk first to define Vulkan types

#include <iostream>
#define GLFW_INCLUDE_NONE  // Don't include any API headers
#include <GLFW/glfw3.h>

// Forward declare GLFW Vulkan function
extern "C" {
VkResult glfwCreateWindowSurface(VkInstance instance, GLFWwindow* window,
                                 const VkAllocationCallbacks* allocator,
                                 VkSurfaceKHR* surface);
}
#include <skity/gpu/gpu_context_vk.hpp>
#include <skity/gpu/gpu_render_target.hpp>

#include "src/gpu/vk/gpu_context_impl_vk.hpp"
#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/render/hw/hw_canvas.hpp"

namespace skity {
namespace example {

WindowVK::WindowVK(int width, int height, std::string title)
    : Window(width, height, std::move(title)) {}

bool WindowVK::OnInit() {
  // Set GLFW hints for Vulkan
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  return true;
}

GLFWwindow* WindowVK::CreateWindowHandler() {
  return glfwCreateWindow(GetWidth(), GetHeight(), GetTitle().c_str(), nullptr,
                          nullptr);
}

std::unique_ptr<skity::GPUContext> WindowVK::CreateGPUContext() {
  // Check if Vulkan is available
  if (!skity::IsVulkanAvailable()) {
    std::cerr << "[ERROR]Vulkan is not available on this system." << std::endl;
    return nullptr;
  }

  // Get available devices
  uint32_t device_count = 0;
  const char** devices = skity::VkGetAvailableDevices(&device_count);
  std::cout << "Found " << device_count << " Vulkan devices:" << std::endl;
  if (devices) {
    for (uint32_t i = 0; i < device_count; i++) {
      std::cout << "   Device " << i << ": " << devices[i] << std::endl;
    }
  }

  // Configure Vulkan context with validation layers enabled
  skity::VkDevicePreferences prefs;
  prefs.enable_validation = true;   // Enable validation layers for debugging
  prefs.preferred_device_type = 2;  // Prefer discrete GPU

  // Create Vulkan context with preferences
  auto context = skity::VkContextCreate(prefs);
  if (!context) {
    std::cerr << "[ERROR] Failed to create Vulkan context with validation."
              << std::endl;
    return nullptr;
  }

  // Validate the context backend type
  auto backend_type = context->GetBackendType();
  std::cout << "Context backend type: " << static_cast<int>(backend_type)
            << std::endl;
  return context;
}

void WindowVK::OnShow() {
  // Validate GPU context
  auto* gpu_context = GetGPUContext();
  if (!gpu_context) {
    std::cerr << "[ERROR] No GPU context available for surface creation"
              << std::endl;
    return;
  }

  // Cast to Vulkan-specific implementations
  auto* vk_context_impl = static_cast<GPUContextImplVk*>(gpu_context);
  auto* vk_device = static_cast<GPUDeviceVk*>(vk_context_impl->GetGPUDevice());

  // Try to create VkSurfaceKHR from GLFW window using the new API
  uint64_t instance_handle = VkGetInstance(gpu_context);

  if (instance_handle == 0) {
    std::cerr << "[ERROR] Failed to get VkInstance from context" << std::endl;
    exit(0);
    return;
  }

  VkInstance instance = reinterpret_cast<VkInstance>(instance_handle);
  if (instance == VK_NULL_HANDLE) {
    std::cerr << "[ERROR] VkInstance is VK_NULL_HANDLE" << std::endl;
  }

  // Initialize volk if not already done
  VkResult volk_result = volkInitialize();
  if (volk_result != VK_SUCCESS) {
    std::cerr << "[ERROR] Failed to initialize volk: " << volk_result
              << std::endl;
    exit(0);
    return;
  }

  volkLoadInstance(instance);  // Load instance-specific functions

  // Check what extensions GLFW requires
  uint32_t glfw_extension_count = 0;
  const char** glfw_extensions =
      glfwGetRequiredInstanceExtensions(&glfw_extension_count);
  std::cout << "GLFW requires " << glfw_extension_count
            << " extensions:" << std::endl;
  for (uint32_t i = 0; i < glfw_extension_count; i++) {
    std::cout << "   - " << glfw_extensions[i] << std::endl;
  }

  std::cout << "Creating window surface..." << std::endl;
  VkResult result = glfwCreateWindowSurface(instance, GetNativeWindow(),
                                            nullptr, &vk_surface_);
  if (result != VK_SUCCESS) {
    std::cerr << "[ERROR] Failed to create window surface: " << result
              << std::endl;
    exit(0);
    return;
  }

  GPUSurfaceDescriptorVk vk_desc{};
  vk_desc.backend = GPUBackendType::kVulkan;
  vk_desc.width = GetWidth();
  vk_desc.height = GetHeight();
  vk_desc.sample_count = 1;
  vk_desc.content_scale = 1.0f;
  vk_desc.surface_type = VkSurfaceType::kSwapchain;
  vk_desc.native_surface = reinterpret_cast<void*>(vk_surface_);

  window_surface_ = gpu_context->CreateSurface(&vk_desc);
  if (!window_surface_) {
    std::cerr << "[ERROR] Failed to create GPUSurface with swapchain"
              << std::endl;
    exit(0);
    return;
  }
}

skity::Canvas* WindowVK::AquireCanvas() {
  if (window_surface_) {
    auto* canvas = window_surface_->LockCanvas(false);
    return canvas;
  }
  std::cerr << "[ERROR] No render surface available." << std::endl;
  return nullptr;
}

void WindowVK::OnPresent() {
  if (window_surface_) {
    window_surface_->Flush();
    return;
  }
  std::cerr << "[ERROR] No render surface available for present" << std::endl;
}

void WindowVK::OnTerminate() {
  // Clean up resources
  window_surface_.reset();

  // Destroy VkSurfaceKHR if created
  if (vk_surface_ != VK_NULL_HANDLE) {
    auto* gpu_context = GetGPUContext();
    if (gpu_context) {
      uint64_t instance_handle = VkGetInstance(gpu_context);
      if (instance_handle != 0) {
        VkInstance instance = reinterpret_cast<VkInstance>(instance_handle);
        volkLoadInstance(instance);  // Ensure volk functions are loaded
        vkDestroySurfaceKHR(instance, vk_surface_, nullptr);
      }
    }
    vk_surface_ = VK_NULL_HANDLE;
  }

  std::cout << "Vulkan window terminated." << std::endl;
}

}  // namespace example
}  // namespace skity