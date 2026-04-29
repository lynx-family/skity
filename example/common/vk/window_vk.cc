// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/vk/window_vk.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>

namespace skity {
namespace example {

namespace {

#ifndef SKITY_EXAMPLE_VK_ENABLE_VALIDATION
#if defined(NDEBUG)
#define SKITY_EXAMPLE_VK_ENABLE_VALIDATION 0
#else
#define SKITY_EXAMPLE_VK_ENABLE_VALIDATION 1
#endif
#endif

constexpr char kPortabilitySubsetExtensionName[] = "VK_KHR_portability_subset";
constexpr char kValidationLayerName[] = "VK_LAYER_KHRONOS_validation";
constexpr uint32_t kRequiredVulkanApiVersion = VK_API_VERSION_1_1;

bool ContainsLayer(const std::vector<VkLayerProperties>& layers,
                   const char* name) {
  return std::any_of(layers.begin(), layers.end(), [name](const auto& layer) {
    return std::string(layer.layerName) == name;
  });
}

bool ContainsExtension(const std::vector<VkExtensionProperties>& extensions,
                       const char* name) {
  return std::any_of(extensions.begin(), extensions.end(),
                     [name](const auto& extension) {
                       return std::string(extension.extensionName) == name;
                     });
}

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

void AddUniqueLayer(std::vector<const char*>* layers, const char* name) {
  if (layers == nullptr || name == nullptr) {
    return;
  }

  if (std::find(layers->begin(), layers->end(), name) != layers->end()) {
    return;
  }

  layers->push_back(name);
}

VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  (void)message_severity;
  (void)message_types;
  (void)user_data;

  const char* message =
      callback_data != nullptr && callback_data->pMessage != nullptr
          ? callback_data->pMessage
          : "Unknown Vulkan validation message";
  std::cerr << "[Vulkan Validation] " << message << std::endl;
  return VK_FALSE;
}

}  // namespace

bool WindowVK::OnInit() {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  return true;
}

GLFWwindow* WindowVK::CreateWindowHandler() {
  return glfwCreateWindow(GetWidth(), GetHeight(), GetTitle().c_str(), nullptr,
                          nullptr);
}

std::unique_ptr<skity::GPUContext> WindowVK::CreateGPUContext() {
  if (!CreateInstance() || !CreateSurface() ||
      !PickPhysicalDeviceAndQueueFamily() || !CreateLogicalDevice()) {
    return nullptr;
  }

  skity::GPUContextInfoVK info = {};
  info.instance = instance_;
  info.get_instance_proc_addr = vkGetInstanceProcAddr;
  info.enabled_instance_extensions = enabled_instance_extensions_.data();
  info.enabled_instance_extension_count =
      static_cast<uint32_t>(enabled_instance_extensions_.size());
  info.enabled_instance_extensions_known = true;
  info.physical_device = physical_device_;
  info.logical_device = device_;
  info.get_device_proc_addr = vkGetDeviceProcAddr;
  info.enabled_device_extensions = enabled_device_extensions_.data();
  info.enabled_device_extension_count =
      static_cast<uint32_t>(enabled_device_extensions_.size());
  info.enabled_device_extensions_known = true;
  info.graphics_queue = graphics_queue_;
  info.graphics_queue_family_index =
      static_cast<int32_t>(graphics_queue_family_index_);

  return skity::CreateGPUContextVK(&info);
}

void WindowVK::OnShow() {
  int framebuffer_width = 0;
  int framebuffer_height = 0;
  glfwGetFramebufferSize(GetNativeWindow(), &framebuffer_width,
                         &framebuffer_height);

  skity::GPUPresenterDescriptorVK desc = {};
  desc.backend = skity::GPUBackendType::kVulkan;
  desc.width = static_cast<uint32_t>(std::max(framebuffer_width, 1));
  desc.height = static_cast<uint32_t>(std::max(framebuffer_height, 1));
  desc.surface = surface_khr_;
  desc.present_queue = graphics_queue_;
  desc.present_queue_family_index =
      static_cast<int32_t>(graphics_queue_family_index_);
  desc.min_image_count = 2;
  desc.format = VK_FORMAT_B8G8R8A8_UNORM;
  desc.color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  desc.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

  presenter_ = GetGPUContext()->CreatePresenter(&desc);
  if (presenter_ == nullptr) {
    std::cerr << "Failed to create Vulkan presenter." << std::endl;
    glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
  }
}

skity::Canvas* WindowVK::AquireCanvas() {
  if (presenter_ == nullptr) {
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

  auto acquire_result = presenter_->AcquireNextSurface(acquire_desc);
  if (acquire_result.status == skity::GPUPresenterStatus::kNeedRecreate) {
    glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
    return nullptr;
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
  if (canvas_ == nullptr || surface_ == nullptr) {
    return;
  }

  canvas_->Flush();
  surface_->Flush();
  canvas_ = nullptr;

  const auto present_status = presenter_->Present(std::move(surface_));
  if (present_status == skity::GPUPresenterStatus::kNeedRecreate) {
    glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
  }
}

void WindowVK::OnTerminate() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
  }

  canvas_ = nullptr;
  surface_.reset();
  presenter_.reset();

  ResetGPUContext();

  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }

  if (surface_khr_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_khr_, nullptr);
    surface_khr_ = VK_NULL_HANDLE;
  }

  if (debug_messenger_ != VK_NULL_HANDLE) {
    vkDestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
    debug_messenger_ = VK_NULL_HANDLE;
  }

  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
  }
}

bool WindowVK::CreateInstance() {
  if (volkInitialize() != VK_SUCCESS) {
    std::cerr << "Failed to initialize volk." << std::endl;
    return false;
  }

  uint32_t supported_instance_version = VK_API_VERSION_1_0;
  if (vkEnumerateInstanceVersion != nullptr &&
      vkEnumerateInstanceVersion(&supported_instance_version) != VK_SUCCESS) {
    std::cerr << "Failed to query Vulkan instance version." << std::endl;
    return false;
  }

  if (VK_API_VERSION_MAJOR(supported_instance_version) <
          VK_API_VERSION_MAJOR(kRequiredVulkanApiVersion) ||
      (VK_API_VERSION_MAJOR(supported_instance_version) ==
           VK_API_VERSION_MAJOR(kRequiredVulkanApiVersion) &&
       VK_API_VERSION_MINOR(supported_instance_version) <
           VK_API_VERSION_MINOR(kRequiredVulkanApiVersion))) {
    std::cerr << "Vulkan 1.1 instance support is required, but only Vulkan "
              << VK_API_VERSION_MAJOR(supported_instance_version) << "."
              << VK_API_VERSION_MINOR(supported_instance_version)
              << " is available." << std::endl;
    return false;
  }

  uint32_t required_extension_count = 0;
  const char** required_extensions =
      glfwGetRequiredInstanceExtensions(&required_extension_count);
  if (required_extensions == nullptr || required_extension_count == 0) {
    std::cerr << "Failed to query GLFW Vulkan instance extensions."
              << std::endl;
    return false;
  }

  enabled_instance_extensions_.assign(
      required_extensions, required_extensions + required_extension_count);
  enabled_instance_layers_.clear();

  uint32_t extension_count = 0;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                             nullptr) != VK_SUCCESS) {
    return false;
  }

  std::vector<VkExtensionProperties> extensions(extension_count);
  if (extension_count > 0 &&
      vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                             extensions.data()) != VK_SUCCESS) {
    return false;
  }

  uint32_t layer_count = 0;
  if (vkEnumerateInstanceLayerProperties(&layer_count, nullptr) != VK_SUCCESS) {
    return false;
  }

  std::vector<VkLayerProperties> layers(layer_count);
  if (layer_count > 0 && vkEnumerateInstanceLayerProperties(
                             &layer_count, layers.data()) != VK_SUCCESS) {
    return false;
  }

  VkInstanceCreateFlags create_flags = 0;
  if (ContainsExtension(extensions,
                        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    AddUniqueExtension(&enabled_instance_extensions_,
                       VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    create_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }

#if SKITY_EXAMPLE_VK_ENABLE_VALIDATION
  if (ContainsExtension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    AddUniqueExtension(&enabled_instance_extensions_,
                       VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  } else {
    std::cerr << "Vulkan debug extension " << VK_EXT_DEBUG_UTILS_EXTENSION_NAME
              << " is unavailable, skipping debug messenger." << std::endl;
  }

  if (ContainsLayer(layers, kValidationLayerName)) {
    AddUniqueLayer(&enabled_instance_layers_, kValidationLayerName);
  } else {
    std::cerr << "Vulkan validation layer " << kValidationLayerName
              << " is unavailable, continuing without validation." << std::endl;
  }
#endif

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Skity Example";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "Skity";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = kRequiredVulkanApiVersion;

  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.flags = create_flags;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(enabled_instance_extensions_.size());
  create_info.ppEnabledExtensionNames = enabled_instance_extensions_.data();
  create_info.enabledLayerCount =
      static_cast<uint32_t>(enabled_instance_layers_.size());
  create_info.ppEnabledLayerNames = enabled_instance_layers_.data();

  if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS ||
      instance_ == VK_NULL_HANDLE) {
    std::cerr << "Failed to create Vulkan instance." << std::endl;
    return false;
  }

  volkLoadInstance(instance_);

#if SKITY_EXAMPLE_VK_ENABLE_VALIDATION
  if (std::find(enabled_instance_extensions_.begin(),
                enabled_instance_extensions_.end(),
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME) !=
      enabled_instance_extensions_.end()) {
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
    debug_create_info.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_create_info.pfnUserCallback = DebugUtilsMessengerCallback;

    if (vkCreateDebugUtilsMessengerEXT(instance_, &debug_create_info, nullptr,
                                       &debug_messenger_) != VK_SUCCESS) {
      std::cerr << "Failed to create Vulkan debug utils messenger."
                << std::endl;
      debug_messenger_ = VK_NULL_HANDLE;
    }
  }
#endif

  return true;
}

bool WindowVK::CreateSurface() {
  if (glfwCreateWindowSurface(instance_, GetNativeWindow(), nullptr,
                              &surface_khr_) != VK_SUCCESS ||
      surface_khr_ == VK_NULL_HANDLE) {
    std::cerr << "Failed to create Vulkan window surface." << std::endl;
    return false;
  }

  return true;
}

bool WindowVK::PickPhysicalDeviceAndQueueFamily() {
  uint32_t physical_device_count = 0;
  if (vkEnumeratePhysicalDevices(instance_, &physical_device_count, nullptr) !=
          VK_SUCCESS ||
      physical_device_count == 0) {
    return false;
  }

  std::vector<VkPhysicalDevice> physical_devices(physical_device_count,
                                                 VK_NULL_HANDLE);
  if (vkEnumeratePhysicalDevices(instance_, &physical_device_count,
                                 physical_devices.data()) != VK_SUCCESS) {
    return false;
  }

  for (auto physical_device : physical_devices) {
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    if (VK_API_VERSION_MAJOR(properties.apiVersion) <
            VK_API_VERSION_MAJOR(kRequiredVulkanApiVersion) ||
        (VK_API_VERSION_MAJOR(properties.apiVersion) ==
             VK_API_VERSION_MAJOR(kRequiredVulkanApiVersion) &&
         VK_API_VERSION_MINOR(properties.apiVersion) <
             VK_API_VERSION_MINOR(kRequiredVulkanApiVersion))) {
      continue;
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_count, nullptr);
    if (queue_family_count == 0) {
      continue;
    }

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, queue_families.data());

    for (uint32_t index = 0; index < queue_family_count; ++index) {
      VkBool32 present_supported = VK_FALSE;
      if (vkGetPhysicalDeviceSurfaceSupportKHR(
              physical_device, index, surface_khr_, &present_supported) !=
          VK_SUCCESS) {
        continue;
      }

      const bool graphics_supported =
          (queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
      if (!graphics_supported || present_supported != VK_TRUE) {
        continue;
      }

      physical_device_ = physical_device;
      graphics_queue_family_index_ = index;
      return true;
    }
  }

  return false;
}

bool WindowVK::CreateLogicalDevice() {
  uint32_t extension_count = 0;
  if (vkEnumerateDeviceExtensionProperties(
          physical_device_, nullptr, &extension_count, nullptr) != VK_SUCCESS) {
    return false;
  }

  std::vector<VkExtensionProperties> extensions(extension_count);
  if (extension_count > 0 && vkEnumerateDeviceExtensionProperties(
                                 physical_device_, nullptr, &extension_count,
                                 extensions.data()) != VK_SUCCESS) {
    return false;
  }

  enabled_device_extensions_.clear();
  AddUniqueExtension(&enabled_device_extensions_,
                     VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  if (ContainsExtension(extensions, kPortabilitySubsetExtensionName)) {
    AddUniqueExtension(&enabled_device_extensions_,
                       kPortabilitySubsetExtensionName);
  }

  float queue_priority = 1.f;
  VkDeviceQueueCreateInfo queue_create_info = {};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = graphics_queue_family_index_;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  VkDeviceCreateInfo device_create_info = {};
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledExtensionCount =
      static_cast<uint32_t>(enabled_device_extensions_.size());
  device_create_info.ppEnabledExtensionNames =
      enabled_device_extensions_.data();

  if (vkCreateDevice(physical_device_, &device_create_info, nullptr,
                     &device_) != VK_SUCCESS ||
      device_ == VK_NULL_HANDLE) {
    std::cerr << "Failed to create Vulkan logical device." << std::endl;
    return false;
  }

  volkLoadDevice(device_);
  vkGetDeviceQueue(device_, graphics_queue_family_index_, 0, &graphics_queue_);
  return graphics_queue_ != VK_NULL_HANDLE;
}

}  // namespace example
}  // namespace skity
