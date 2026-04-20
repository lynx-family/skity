// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/vulkan_context_state.hpp"

#include <array>
#include <set>
#include <string_view>
#include <vector>

#include "src/logging.hpp"

namespace skity {

namespace {

uint32_t ResolveInstanceApiVersion(const VulkanGlobalFns& global_fns) {
  if (global_fns.vkEnumerateInstanceVersion == nullptr) {
    return VK_API_VERSION_1_0;
  }

  uint32_t version = VK_API_VERSION_1_0;
  if (global_fns.vkEnumerateInstanceVersion(&version) != VK_SUCCESS) {
    return VK_API_VERSION_1_0;
  }

  return version;
}

bool IsAtLeastVulkan11(uint32_t version) {
  return VK_API_VERSION_MAJOR(version) > 1 ||
         (VK_API_VERSION_MAJOR(version) == 1 &&
          VK_API_VERSION_MINOR(version) >= 1);
}

bool IsApiVersionGreater(uint32_t lhs, uint32_t rhs) {
  if (VK_API_VERSION_MAJOR(lhs) != VK_API_VERSION_MAJOR(rhs)) {
    return VK_API_VERSION_MAJOR(lhs) > VK_API_VERSION_MAJOR(rhs);
  }

  if (VK_API_VERSION_MINOR(lhs) != VK_API_VERSION_MINOR(rhs)) {
    return VK_API_VERSION_MINOR(lhs) > VK_API_VERSION_MINOR(rhs);
  }

  if (VK_API_VERSION_PATCH(lhs) != VK_API_VERSION_PATCH(rhs)) {
    return VK_API_VERSION_PATCH(lhs) > VK_API_VERSION_PATCH(rhs);
  }

  return VK_API_VERSION_VARIANT(lhs) > VK_API_VERSION_VARIANT(rhs);
}

const char* VulkanVendorName(uint32_t vendor_id) {
  switch (vendor_id) {
    case 0x1002:
      return "AMD";
    case 0x1010:
      return "ImgTec";
    case 0x10DE:
      return "NVIDIA";
    case 0x13B5:
      return "ARM";
    case 0x5143:
      return "Qualcomm";
    case 0x8086:
      return "Intel";
    default:
      return "Unknown";
  }
}

bool ContainsExtension(const std::vector<std::string>& extensions,
                       const char* extension_name) {
  if (extension_name == nullptr) {
    return false;
  }

  for (const auto& extension : extensions) {
    if (extension == extension_name) {
      return true;
    }
  }

  return false;
}

bool ContainsExtension(const std::vector<VkExtensionProperties>& extensions,
                       const char* extension_name) {
  if (extension_name == nullptr) {
    return false;
  }

  for (const auto& extension : extensions) {
    if (std::string_view(extension.extensionName) == extension_name) {
      return true;
    }
  }

  return false;
}

void LogExtensions(const char* label,
                   const std::vector<VkExtensionProperties>& extensions) {
  LOGD("Enumerated {} Vulkan {} extension(s)", extensions.size(), label);
  for (const auto& extension : extensions) {
    LOGD("Vulkan {} extension: {} (spec version {})", label,
         extension.extensionName, extension.specVersion);
  }
}

std::vector<std::string> CopyEnabledExtensions(const char* const* extensions,
                                               uint32_t extension_count) {
  std::vector<std::string> result;
  if (extensions == nullptr || extension_count == 0) {
    return result;
  }

  result.reserve(extension_count);
  for (uint32_t i = 0; i < extension_count; ++i) {
    if (extensions[i] != nullptr) {
      result.emplace_back(extensions[i]);
    }
  }

  return result;
}

std::vector<const char*> BuildExtensionNamePtrs(
    const std::vector<std::string>& extensions) {
  std::vector<const char*> result;
  result.reserve(extensions.size());
  for (const auto& extension : extensions) {
    result.push_back(extension.c_str());
  }
  return result;
}

void LogEnabledExtensions(const char* label,
                          const std::vector<std::string>& extensions) {
  if (extensions.empty()) {
    LOGD("Enabled 0 Vulkan {} extension(s)", label);
    return;
  }

  LOGD("Enabled {} Vulkan {} extension(s)", extensions.size(), label);
  for (const auto& extension : extensions) {
    LOGD("Enabled Vulkan {} extension: {}", label, extension);
  }
}

void TryEnableInstanceExtension(
    std::vector<std::string>* enabled_extensions,
    const std::vector<VkExtensionProperties>& available_extensions,
    const char* extension_name) {
  if (enabled_extensions == nullptr || extension_name == nullptr) {
    return;
  }

  if (!ContainsExtension(available_extensions, extension_name) ||
      ContainsExtension(*enabled_extensions, extension_name)) {
    return;
  }

  enabled_extensions->emplace_back(extension_name);
}

}  // namespace

VulkanContextState::~VulkanContextState() { Reset(); }

bool VulkanContextState::Initialize(const GPUContextInfoVK& info) {
  Reset();
  LOGD("Initializing VulkanContextState");

  if (!InitializeInstance(info) || !InitializePhysicalDevice(info) ||
      !InitializeQueues(info) || !InitializeDevice(info)) {
    LOGE("Failed to initialize VulkanContextState");
    Reset();
    return false;
  }

  LOGD("Initialized VulkanContextState successfully");
  return true;
}

bool VulkanContextState::HasAvailableInstanceExtension(
    const char* extension_name) const {
  return ContainsExtension(available_instance_extensions_, extension_name);
}

bool VulkanContextState::HasEnabledInstanceExtension(
    const char* extension_name) const {
  return ContainsExtension(enabled_instance_extensions_, extension_name);
}

bool VulkanContextState::HasAvailableDeviceExtension(
    const char* extension_name) const {
  return ContainsExtension(available_device_extensions_, extension_name);
}

bool VulkanContextState::HasEnabledDeviceExtension(
    const char* extension_name) const {
  return ContainsExtension(enabled_device_extensions_, extension_name);
}

int32_t VulkanContextState::FindQueueFamilyIndex(VkQueueFlags flags,
                                                 bool prefer_dedicated) const {
  int32_t fallback_index = -1;

  for (size_t i = 0; i < queue_family_properties_.size(); ++i) {
    const auto& properties = queue_family_properties_[i];
    if ((properties.queueFlags & flags) != flags ||
        properties.queueCount == 0) {
      continue;
    }

    if (!prefer_dedicated) {
      return static_cast<int32_t>(i);
    }

    const bool is_dedicated =
        (flags == VK_QUEUE_COMPUTE_BIT &&
         (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) ||
        (flags == VK_QUEUE_TRANSFER_BIT &&
         (properties.queueFlags &
          (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == 0);
    if (is_dedicated) {
      return static_cast<int32_t>(i);
    }

    if (fallback_index < 0) {
      fallback_index = static_cast<int32_t>(i);
    }
  }

  return fallback_index;
}

bool VulkanContextState::InitializeInstance(const GPUContextInfoVK& info) {
  functions_.get_instance_proc_addr = info.get_instance_proc_addr;
  if (functions_.get_instance_proc_addr == nullptr) {
    LOGE("Failed to initialize Vulkan instance: vkGetInstanceProcAddr is null");
    return false;
  }

  if (!LoadVulkanGlobalFns(functions_.get_instance_proc_addr,
                           &functions_.global)) {
    return false;
  }

  if (!LoadAvailableInstanceExtensions()) {
    return false;
  }

  if (info.instance != VK_NULL_HANDLE) {
    instance_ = info.instance;
    LOGD("Using user provided Vulkan instance: {:p}",
         reinterpret_cast<void*>(instance_));
    if (!LoadVulkanInstanceFns(functions_.get_instance_proc_addr, instance_,
                               &functions_.instance)) {
      LOGE(
          "Failed to initialize Vulkan instance procedures from user instance");
      return false;
    }

    enabled_instance_extensions_ =
        CopyEnabledExtensions(info.enabled_instance_extensions,
                              info.enabled_instance_extension_count);
    enabled_instance_extensions_known_ = info.enabled_instance_extensions_known;
    if (enabled_instance_extensions_known_) {
      LogEnabledExtensions("instance", enabled_instance_extensions_);
    } else {
      LOGW(
          "Enabled Vulkan instance extensions are unknown for user provided "
          "instance");
    }
  } else {
    enabled_instance_extensions_.clear();
    TryEnableInstanceExtension(&enabled_instance_extensions_,
                               available_instance_extensions_,
                               VK_KHR_SURFACE_EXTENSION_NAME);
    TryEnableInstanceExtension(
        &enabled_instance_extensions_, available_instance_extensions_,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    VkInstanceCreateFlags instance_flags = 0;
    if (HasAvailableInstanceExtension(
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
      TryEnableInstanceExtension(&enabled_instance_extensions_,
                                 available_instance_extensions_,
                                 VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
      instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

#if defined(SKITY_ANDROID)
#if defined(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)
    TryEnableInstanceExtension(&enabled_instance_extensions_,
                               available_instance_extensions_,
                               VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif
#elif defined(SKITY_WIN)
#if defined(VK_KHR_WIN32_SURFACE_EXTENSION_NAME)
    TryEnableInstanceExtension(&enabled_instance_extensions_,
                               available_instance_extensions_,
                               VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
#elif defined(SKITY_MACOS) || defined(SKITY_IOS)
#if defined(VK_EXT_METAL_SURFACE_EXTENSION_NAME)
    TryEnableInstanceExtension(&enabled_instance_extensions_,
                               available_instance_extensions_,
                               VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#endif
#elif defined(SKITY_LINUX)
#if defined(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME)
    TryEnableInstanceExtension(&enabled_instance_extensions_,
                               available_instance_extensions_,
                               VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_KHR_XCB_SURFACE_EXTENSION_NAME)
    TryEnableInstanceExtension(&enabled_instance_extensions_,
                               available_instance_extensions_,
                               VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_KHR_XLIB_SURFACE_EXTENSION_NAME)
    TryEnableInstanceExtension(&enabled_instance_extensions_,
                               available_instance_extensions_,
                               VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#endif

    auto enabled_extension_ptrs =
        BuildExtensionNamePtrs(enabled_instance_extensions_);

    const uint32_t target_version =
        ResolveInstanceApiVersion(functions_.global);
    if (!IsAtLeastVulkan11(target_version)) {
      LOGE("Vulkan 1.1 is required, but only {}.{} is available",
           VK_API_VERSION_MAJOR(target_version),
           VK_API_VERSION_MINOR(target_version));
      return false;
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "skity";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "skity";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = target_version;

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.flags = instance_flags;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount =
        static_cast<uint32_t>(enabled_extension_ptrs.size());
    instance_info.ppEnabledExtensionNames = enabled_extension_ptrs.data();

    own_instance_ = CreateVkInstance(functions_.get_instance_proc_addr,
                                     &instance_, &functions_, &instance_info);
    if (!own_instance_) {
      LOGE("Failed to create owned Vulkan instance");
      return false;
    }

    LOGD("Created owned Vulkan instance: {:p}",
         reinterpret_cast<void*>(instance_));
    enabled_instance_extensions_known_ = true;
    LogEnabledExtensions("instance", enabled_instance_extensions_);
  }

  return true;
}

bool VulkanContextState::InitializePhysicalDevice(
    const GPUContextInfoVK& info) {
  if (info.physical_device != VK_NULL_HANDLE) {
    physical_device_ = info.physical_device;
    VkPhysicalDeviceProperties properties = {};
    functions_.instance.vkGetPhysicalDeviceProperties(physical_device_,
                                                      &properties);
    LOGD("Using user provided Vulkan physical device: {:p}",
         reinterpret_cast<void*>(physical_device_));
    LOGD("User physical device API version: {}.{}.{}",
         VK_API_VERSION_MAJOR(properties.apiVersion),
         VK_API_VERSION_MINOR(properties.apiVersion),
         VK_API_VERSION_PATCH(properties.apiVersion));
  } else {
    uint32_t physical_device_count = 0;
    VkResult result = functions_.instance.vkEnumeratePhysicalDevices(
        instance_, &physical_device_count, nullptr);
    if (result != VK_SUCCESS || physical_device_count == 0) {
      LOGE("Failed to enumerate Vulkan physical devices: result={}, count={}",
           static_cast<int32_t>(result), physical_device_count);
      return false;
    }

    LOGD("Enumerated {} Vulkan physical device(s)", physical_device_count);

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count,
                                                   VK_NULL_HANDLE);
    result = functions_.instance.vkEnumeratePhysicalDevices(
        instance_, &physical_device_count, physical_devices.data());
    if (result != VK_SUCCESS) {
      LOGE("Failed to fetch Vulkan physical devices: result={}",
           static_cast<int32_t>(result));
      return false;
    }

    uint32_t selected_api_version = 0;
    for (auto physical_device : physical_devices) {
      VkPhysicalDeviceProperties properties = {};
      functions_.instance.vkGetPhysicalDeviceProperties(physical_device,
                                                        &properties);
      LOGD("Found Vulkan physical device: {:p}, apiVersion: {}.{}.{}",
           reinterpret_cast<void*>(physical_device),
           VK_API_VERSION_MAJOR(properties.apiVersion),
           VK_API_VERSION_MINOR(properties.apiVersion),
           VK_API_VERSION_PATCH(properties.apiVersion));

      if (physical_device_ == VK_NULL_HANDLE ||
          IsApiVersionGreater(properties.apiVersion, selected_api_version)) {
        physical_device_ = physical_device;
        selected_api_version = properties.apiVersion;
      }
    }

    LOGD(
        "Selected Vulkan physical device with highest API version: {:p}, "
        "{}.{}.{}",
        reinterpret_cast<void*>(physical_device_),
        VK_API_VERSION_MAJOR(selected_api_version),
        VK_API_VERSION_MINOR(selected_api_version),
        VK_API_VERSION_PATCH(selected_api_version));
  }

  if (physical_device_ == VK_NULL_HANDLE) {
    return false;
  }

  uint32_t queue_family_count = 0;
  functions_.instance.vkGetPhysicalDeviceQueueFamilyProperties(
      physical_device_, &queue_family_count, nullptr);
  if (queue_family_count == 0) {
    LOGE("Failed to query Vulkan queue families: no queue family available");
    return false;
  }

  LOGD("Physical device {:p} exposes {} queue family(s)",
       reinterpret_cast<void*>(physical_device_), queue_family_count);

  queue_family_properties_.resize(queue_family_count);
  functions_.instance.vkGetPhysicalDeviceQueueFamilyProperties(
      physical_device_, &queue_family_count, queue_family_properties_.data());
  return true;
}

bool VulkanContextState::InitializeQueues(const GPUContextInfoVK& info) {
  graphics_queue_family_index_ =
      info.graphics_queue_family_index >= 0
          ? info.graphics_queue_family_index
          : FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT, false);
  compute_queue_family_index_ =
      info.compute_queue_family_index >= 0
          ? info.compute_queue_family_index
          : FindQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT, true);
  transfer_queue_family_index_ =
      info.transfer_queue_family_index >= 0
          ? info.transfer_queue_family_index
          : FindQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT, true);

  if (graphics_queue_family_index_ < 0) {
    LOGE("Failed to find Vulkan graphics queue family");
    return false;
  }

  if (compute_queue_family_index_ < 0) {
    compute_queue_family_index_ = graphics_queue_family_index_;
  }

  if (transfer_queue_family_index_ < 0) {
    transfer_queue_family_index_ = compute_queue_family_index_;
  }

  LOGD("Using Vulkan queue families, graphics: {}, compute: {}, transfer: {}",
       graphics_queue_family_index_, compute_queue_family_index_,
       transfer_queue_family_index_);
  return true;
}

bool VulkanContextState::LoadAvailableInstanceExtensions() {
  available_instance_extensions_.clear();
  if (functions_.global.vkEnumerateInstanceExtensionProperties == nullptr) {
    LOGE("Failed to enumerate Vulkan instance extensions: loader is null");
    return false;
  }

  uint32_t extension_count = 0;
  VkResult result = functions_.global.vkEnumerateInstanceExtensionProperties(
      nullptr, &extension_count, nullptr);
  if (result != VK_SUCCESS) {
    LOGE("Failed to query Vulkan instance extension count: {}",
         static_cast<int32_t>(result));
    return false;
  }

  if (extension_count > 0) {
    available_instance_extensions_.resize(extension_count);
    result = functions_.global.vkEnumerateInstanceExtensionProperties(
        nullptr, &extension_count, available_instance_extensions_.data());
    if (result != VK_SUCCESS) {
      LOGE("Failed to query Vulkan instance extensions: {}",
           static_cast<int32_t>(result));
      available_instance_extensions_.clear();
      return false;
    }
    available_instance_extensions_.resize(extension_count);
  }

  LogExtensions("instance", available_instance_extensions_);
  return true;
}

bool VulkanContextState::LoadAvailableDeviceExtensions() {
  available_device_extensions_.clear();
  if (functions_.instance.vkEnumerateDeviceExtensionProperties == nullptr) {
    LOGE("Failed to enumerate Vulkan device extensions: loader is null");
    return false;
  }

  uint32_t extension_count = 0;
  VkResult result = functions_.instance.vkEnumerateDeviceExtensionProperties(
      physical_device_, nullptr, &extension_count, nullptr);
  if (result != VK_SUCCESS) {
    LOGE("Failed to query Vulkan device extension count: {}",
         static_cast<int32_t>(result));
    return false;
  }

  if (extension_count > 0) {
    available_device_extensions_.resize(extension_count);
    result = functions_.instance.vkEnumerateDeviceExtensionProperties(
        physical_device_, nullptr, &extension_count,
        available_device_extensions_.data());
    if (result != VK_SUCCESS) {
      LOGE("Failed to query Vulkan device extensions: {}",
           static_cast<int32_t>(result));
      available_device_extensions_.clear();
      return false;
    }
    available_device_extensions_.resize(extension_count);
  }

  LogExtensions("device", available_device_extensions_);
  return true;
}

bool VulkanContextState::LoadDeviceFns() {
  if (functions_.get_device_proc_addr == nullptr) {
    LOGE(
        "Failed to load Vulkan device procedures: vkGetDeviceProcAddr is null");
    return false;
  }

  return LoadVulkanDeviceFns(functions_.get_device_proc_addr, logical_device_,
                             &functions_.device);
}

bool VulkanContextState::InitializeDevice(const GPUContextInfoVK& info) {
  enabled_device_extensions_.clear();
  enabled_device_extensions_known_ = false;

  if (!LoadAvailableDeviceExtensions()) {
    return false;
  }

  if (info.logical_device != VK_NULL_HANDLE) {
    logical_device_ = info.logical_device;
    functions_.get_device_proc_addr =
        info.get_device_proc_addr != nullptr
            ? info.get_device_proc_addr
            : functions_.instance.vkGetDeviceProcAddr;
    LOGD("Using user provided Vulkan logical device: {:p}",
         reinterpret_cast<void*>(logical_device_));
    if (!LoadDeviceFns()) {
      LOGE("Failed to initialize procedures from user Vulkan logical device");
      return false;
    }
    enabled_device_extensions_ = CopyEnabledExtensions(
        info.enabled_device_extensions, info.enabled_device_extension_count);
    enabled_device_extensions_known_ = info.enabled_device_extensions_known;
    if (enabled_device_extensions_known_) {
      LogEnabledExtensions("device", enabled_device_extensions_);
    } else {
      LOGW(
          "Enabled Vulkan device extensions are unknown for user provided "
          "logical device");
    }
  } else {
    if (HasAvailableDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
      enabled_device_extensions_.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    if (HasAvailableDeviceExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)) {
      enabled_device_extensions_.emplace_back(
          VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    }
    if (HasAvailableDeviceExtension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)) {
      enabled_device_extensions_.emplace_back(
          VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
    }
    if (HasAvailableDeviceExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
      enabled_device_extensions_.emplace_back(
          VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    }
    if (HasAvailableDeviceExtension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME)) {
      enabled_device_extensions_.emplace_back(
          VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    }

    auto enabled_extension_ptrs =
        BuildExtensionNamePtrs(enabled_device_extensions_);

    static constexpr float kQueuePriority = 1.0f;
    const std::array<int32_t, 3> queue_family_indices = {
        graphics_queue_family_index_, compute_queue_family_index_,
        transfer_queue_family_index_};

    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    std::set<uint32_t> unique_family_indices;
    for (int32_t family_index : queue_family_indices) {
      if (family_index < 0) {
        continue;
      }

      if (!unique_family_indices.insert(static_cast<uint32_t>(family_index))
               .second) {
        continue;
      }

      VkDeviceQueueCreateInfo queue_info = {};
      queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_info.queueFamilyIndex = static_cast<uint32_t>(family_index);
      queue_info.queueCount = 1;
      queue_info.pQueuePriorities = &kQueuePriority;
      queue_infos.push_back(queue_info);
    }

    LOGD("Creating Vulkan logical device with {} queue create info entries",
         queue_infos.size());
    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount =
        static_cast<uint32_t>(queue_infos.size());
    device_info.pQueueCreateInfos = queue_infos.data();
    device_info.enabledExtensionCount =
        static_cast<uint32_t>(enabled_extension_ptrs.size());
    device_info.ppEnabledExtensionNames = enabled_extension_ptrs.data();

    VkResult result = functions_.instance.vkCreateDevice(
        physical_device_, &device_info, nullptr, &logical_device_);
    if (result != VK_SUCCESS || logical_device_ == VK_NULL_HANDLE) {
      LOGE("Failed to create Vulkan logical device: result={}, device={:p}",
           static_cast<int32_t>(result),
           reinterpret_cast<void*>(logical_device_));
      return false;
    }

    own_device_ = true;
    functions_.get_device_proc_addr = functions_.instance.vkGetDeviceProcAddr;
    LOGD("Created owned Vulkan logical device: {:p}",
         reinterpret_cast<void*>(logical_device_));
    if (!LoadDeviceFns()) {
      LOGE("Failed to load procedures from created Vulkan logical device");
      return false;
    }
    enabled_device_extensions_known_ = true;
    LogEnabledExtensions("device", enabled_device_extensions_);
  }

  graphics_queue_ = info.graphics_queue;
  compute_queue_ = info.compute_queue;
  transfer_queue_ = info.transfer_queue;

  if (graphics_queue_ == VK_NULL_HANDLE) {
    functions_.device.vkGetDeviceQueue(
        logical_device_, static_cast<uint32_t>(graphics_queue_family_index_), 0,
        &graphics_queue_);
  }

  if (compute_queue_ == VK_NULL_HANDLE) {
    functions_.device.vkGetDeviceQueue(
        logical_device_, static_cast<uint32_t>(compute_queue_family_index_), 0,
        &compute_queue_);
  }

  if (transfer_queue_ == VK_NULL_HANDLE) {
    functions_.device.vkGetDeviceQueue(
        logical_device_, static_cast<uint32_t>(transfer_queue_family_index_), 0,
        &transfer_queue_);
  }

  LOGD("Resolved Vulkan queues, graphics: {:p}, compute: {:p}, transfer: {:p}",
       reinterpret_cast<void*>(graphics_queue_),
       reinterpret_cast<void*>(compute_queue_),
       reinterpret_cast<void*>(transfer_queue_));
  if (graphics_queue_ == VK_NULL_HANDLE) {
    LOGE("Failed to resolve Vulkan graphics queue");
  }

  if (graphics_queue_ != VK_NULL_HANDLE) {
    VkPhysicalDeviceProperties properties = {};
    functions_.instance.vkGetPhysicalDeviceProperties(physical_device_,
                                                      &properties);
    LOGD(
        "Using Vulkan device '{}' , vendor: {} (0x{:04x}), apiVersion: "
        "{}.{}.{}",
        properties.deviceName, VulkanVendorName(properties.vendorID),
        properties.vendorID, VK_API_VERSION_MAJOR(properties.apiVersion),
        VK_API_VERSION_MINOR(properties.apiVersion),
        VK_API_VERSION_PATCH(properties.apiVersion));
  }

  return graphics_queue_ != VK_NULL_HANDLE;
}

void VulkanContextState::Reset() {
  if (own_device_ && logical_device_ != VK_NULL_HANDLE &&
      functions_.device.vkDestroyDevice != nullptr) {
    LOGD("Destroying owned Vulkan logical device: {:p}",
         reinterpret_cast<void*>(logical_device_));
    functions_.device.vkDestroyDevice(logical_device_, nullptr);
  }

  if (own_instance_ && instance_ != VK_NULL_HANDLE &&
      functions_.instance.vkDestroyInstance != nullptr) {
    LOGD("Destroying owned Vulkan instance: {:p}",
         reinterpret_cast<void*>(instance_));
    functions_.instance.vkDestroyInstance(instance_, nullptr);
  }

  queue_family_properties_.clear();
  available_instance_extensions_.clear();
  available_device_extensions_.clear();
  enabled_instance_extensions_.clear();
  enabled_device_extensions_.clear();
  enabled_instance_extensions_known_ = false;
  enabled_device_extensions_known_ = false;
  functions_ = {};
  instance_ = VK_NULL_HANDLE;
  physical_device_ = VK_NULL_HANDLE;
  logical_device_ = VK_NULL_HANDLE;
  graphics_queue_ = VK_NULL_HANDLE;
  compute_queue_ = VK_NULL_HANDLE;
  transfer_queue_ = VK_NULL_HANDLE;
  graphics_queue_family_index_ = -1;
  compute_queue_family_index_ = -1;
  transfer_queue_family_index_ = -1;
  own_instance_ = false;
  own_device_ = false;
}

}  // namespace skity
