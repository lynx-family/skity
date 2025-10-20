// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/vk_interface.hpp"

#include <algorithm>
#include <cstring>

#include "src/logging.hpp"

namespace skity {

const std::vector<const char*> VkInterface::kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"};

VkInterface::VkInterface() = default;

VkInterface::~VkInterface() {
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
  }
}

bool VkInterface::Init() {
  // Initialize Volk
  VkResult result = volkInitialize();
  if (result != VK_SUCCESS) {
    LOGE("Failed to initialize Volk: {}", result);
    return false;
  }

  if (!CreateInstance()) {
    return false;
  }

  // Load instance functions
  volkLoadInstance(instance_);

  EnumeratePhysicalDevices();

  return true;
}

bool VkInterface::CreateInstance() {
  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Skity Application";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "Skity";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

  auto extensions = GetRequiredInstanceExtensions();
  create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();

#ifdef SKITY_DEBUG
  validation_layers_enabled_ = CheckValidationLayerSupport();
  if (validation_layers_enabled_) {
    create_info.enabledLayerCount =
        static_cast<uint32_t>(kValidationLayers.size());
    create_info.ppEnabledLayerNames = kValidationLayers.data();
    LOGI("Vulkan validation layers enabled");
  }
#endif

  VkResult result = vkCreateInstance(&create_info, nullptr, &instance_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create Vulkan instance: {}", result);
    return false;
  }

  LOGI("Vulkan instance created successfully");
  return true;
}

void VkInterface::EnumeratePhysicalDevices() {
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

  if (device_count == 0) {
    LOGE("Failed to find GPUs with Vulkan support");
    return;
  }

  physical_devices_.resize(device_count);
  vkEnumeratePhysicalDevices(instance_, &device_count,
                             physical_devices_.data());

  LOGI("Found {} Vulkan physical devices", device_count);
}

VkPhysicalDevice VkInterface::SelectBestPhysicalDevice() const {
  if (physical_devices_.empty()) {
    return VK_NULL_HANDLE;
  }

  // For now, just return the first discrete GPU, or the first device if none
  // found
  for (auto device : physical_devices_) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      LOGI("Selected discrete GPU: {}", properties.deviceName);
      return device;
    }
  }

  // Fall back to first available device
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_devices_[0], &properties);
  LOGI("Selected device: {}", properties.deviceName);
  return physical_devices_[0];
}

bool VkInterface::CheckValidationLayerSupport() const {
  uint32_t layer_count;
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

  std::vector<VkLayerProperties> available_layers(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

  for (const char* layer_name : kValidationLayers) {
    bool layer_found = false;

    for (const auto& layer_properties : available_layers) {
      if (strcmp(layer_name, layer_properties.layerName) == 0) {
        layer_found = true;
        break;
      }
    }

    if (!layer_found) {
      return false;
    }
  }

  return true;
}

std::vector<const char*> VkInterface::GetRequiredInstanceExtensions() const {
  std::vector<const char*> extensions;

  // Add platform-specific extensions
#ifdef VK_USE_PLATFORM_WIN32_KHR
  extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif

  // Common extensions
  extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

  // Add macOS surface extension for GLFW window surfaces
#ifdef __APPLE__
  // Required for MoltenVK portability
  extensions.push_back("VK_EXT_metal_surface");
  extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

#ifdef SKITY_DEBUG
  if (IsValidationLayersAvailable()) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
#endif

  return extensions;
}

std::vector<const char*> VkInterface::GetRequiredDeviceExtensions() const {
  return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

std::vector<const char*> VkInterface::GetRequiredDeviceExtensions(
    VkPhysicalDevice device) const {
  std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  // Check if VK_KHR_portability_subset is available and required
  uint32_t extension_count;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                       nullptr);

  std::vector<VkExtensionProperties> available_extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                       available_extensions.data());

  // Check for portability subset extension (required on MoltenVK)
  const char* portability_extension = "VK_KHR_portability_subset";
  for (const auto& extension : available_extensions) {
    if (strcmp(extension.extensionName, portability_extension) == 0) {
      extensions.push_back(portability_extension);
      LOGI(
          "Added VK_KHR_portability_subset extension for MoltenVK "
          "compatibility");
      break;
    }
  }

  return extensions;
}

bool VkInterface::IsValidationLayersAvailable() const {
  return validation_layers_enabled_;
}

// Global instance
static std::unique_ptr<VkInterface> g_vk_interface;

VkInterface* GetVkInterface() {
  if (!g_vk_interface) {
    g_vk_interface = std::make_unique<VkInterface>();
    if (!g_vk_interface->Init()) {
      g_vk_interface.reset();
      return nullptr;
    }
  }
  return g_vk_interface.get();
}

}  // namespace skity