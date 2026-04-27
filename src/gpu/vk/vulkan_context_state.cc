// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/vulkan_context_state.hpp"

#include <vk_mem_alloc.h>

#include <cstdlib>
#include <set>
#include <string_view>
#include <vector>

#include "src/logging.hpp"

namespace skity {

namespace {

constexpr char kPortabilitySubsetExtensionName[] = "VK_KHR_portability_subset";

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

uint32_t MinApiVersion(uint32_t lhs, uint32_t rhs) {
  return IsApiVersionGreater(lhs, rhs) ? rhs : lhs;
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

bool ContainsLayer(const std::vector<std::string>& layers,
                   const char* layer_name) {
#if !defined(SKITY_VK_DEBUG_RUNTIME)
  (void)layers;
  (void)layer_name;
  return false;
#else
  if (layer_name == nullptr) {
    return false;
  }

  for (const auto& layer : layers) {
    if (layer == layer_name) {
      return true;
    }
  }

  return false;
#endif
}

bool ContainsLayer(const std::vector<VkLayerProperties>& layers,
                   const char* layer_name) {
#if !defined(SKITY_VK_DEBUG_RUNTIME)
  (void)layers;
  (void)layer_name;
  return false;
#else
  if (layer_name == nullptr) {
    return false;
  }

  for (const auto& layer : layers) {
    if (std::string_view(layer.layerName) == layer_name) {
      return true;
    }
  }

  return false;
#endif
}

void LogExtensions(const char* label,
                   const std::vector<VkExtensionProperties>& extensions) {
  LOGD("Enumerated {} Vulkan {} extension(s)", extensions.size(), label);
  for (const auto& extension : extensions) {
    LOGD("Vulkan {} extension: {} (spec version {})", label,
         extension.extensionName, extension.specVersion);
  }
}

void LogLayers(const std::vector<VkLayerProperties>& layers) {
#if !defined(SKITY_VK_DEBUG_RUNTIME)
  (void)layers;
#else
  LOGD("Enumerated {} Vulkan instance layer(s)", layers.size());
  for (const auto& layer : layers) {
    LOGD("Vulkan instance layer: {} (spec version {}, impl version {})",
         layer.layerName, layer.specVersion, layer.implementationVersion);
  }
#endif
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

std::vector<const char*> BuildNamePtrs(const std::vector<std::string>& names) {
  std::vector<const char*> result;
  result.reserve(names.size());
  for (const auto& name : names) {
    result.push_back(name.c_str());
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

void LogEnabledLayers(const std::vector<std::string>& layers) {
#if !defined(SKITY_VK_DEBUG_RUNTIME)
  (void)layers;
#else
  if (layers.empty()) {
    LOGD("Enabled 0 Vulkan instance layer(s)");
    return;
  }

  LOGD("Enabled {} Vulkan instance layer(s)", layers.size());
  for (const auto& layer : layers) {
    LOGD("Enabled Vulkan instance layer: {}", layer);
  }
#endif
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

void TryEnableInstanceLayer(
    std::vector<std::string>* enabled_layers,
    const std::vector<VkLayerProperties>& available_layers,
    const char* layer_name) {
#if !defined(SKITY_VK_DEBUG_RUNTIME)
  (void)enabled_layers;
  (void)available_layers;
  (void)layer_name;
#else
  if (enabled_layers == nullptr || layer_name == nullptr) {
    return;
  }

  if (!ContainsLayer(available_layers, layer_name) ||
      ContainsLayer(*enabled_layers, layer_name)) {
    return;
  }

  enabled_layers->emplace_back(layer_name);
#endif
}

bool ShouldEnableVulkanDebugRuntime(const GPUContextInfoVK& info) {
#if !defined(SKITY_VK_DEBUG_RUNTIME)
  (void)info;
  return false;
#else
  if (info.enable_debug_runtime) {
    return true;
  }

  const char* value = std::getenv("SKITY_VK_DEBUG_RUNTIME");
  if (value == nullptr || *value == '\0') {
    return false;
  }

  const std::string_view flag(value);
  return flag == "1" || flag == "true" || flag == "TRUE" || flag == "on" ||
         flag == "ON" || flag == "yes" || flag == "YES";
#endif
}

bool LoadAvailableInstanceLayersImpl(const VulkanGlobalFns& global_fns,
                                     VulkanDebugRuntimeState* debug_runtime) {
#if !defined(SKITY_VK_DEBUG_RUNTIME)
  (void)global_fns;
  (void)debug_runtime;
  return true;
#else
  if (debug_runtime == nullptr) {
    return true;
  }

  debug_runtime->available_instance_layers.clear();
  debug_runtime->enabled_instance_layers.clear();
  debug_runtime->enabled_instance_layers_known = false;
  if (global_fns.vkEnumerateInstanceLayerProperties == nullptr) {
    LOGE("Failed to enumerate Vulkan instance layers: loader is null");
    return false;
  }

  uint32_t layer_count = 0;
  VkResult result =
      global_fns.vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
  if (result != VK_SUCCESS) {
    LOGE("Failed to query Vulkan instance layer count: {}",
         static_cast<int32_t>(result));
    return false;
  }

  if (layer_count > 0) {
    debug_runtime->available_instance_layers.resize(layer_count);
    result = global_fns.vkEnumerateInstanceLayerProperties(
        &layer_count, debug_runtime->available_instance_layers.data());
    if (result != VK_SUCCESS) {
      LOGE("Failed to query Vulkan instance layers: {}",
           static_cast<int32_t>(result));
      debug_runtime->available_instance_layers.clear();
      return false;
    }
    debug_runtime->available_instance_layers.resize(layer_count);
  }

  LogLayers(debug_runtime->available_instance_layers);
  return true;
#endif
}

#if defined(SKITY_VK_DEBUG_RUNTIME)

void HandleUserProvidedDebugRuntimeHint(const GPUContextInfoVK& info) {
  if (info.enable_debug_runtime) {
    LOGW(
        "GPUContextInfoVK::enable_debug_runtime is ignored for user "
        "provided Vulkan instances");
  }
}

void ConfigureDebugRuntimeForOwnedInstance(
    const GPUContextInfoVK& info,
    const std::vector<VkExtensionProperties>& available_instance_extensions,
    std::vector<std::string>* enabled_instance_extensions,
    VulkanDebugRuntimeState* debug_runtime) {
  if (!ShouldEnableVulkanDebugRuntime(info) ||
      enabled_instance_extensions == nullptr || debug_runtime == nullptr) {
    return;
  }

  if (ContainsExtension(available_instance_extensions,
                        VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    TryEnableInstanceExtension(enabled_instance_extensions,
                               available_instance_extensions,
                               VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  } else {
    LOGW("Vulkan debug runtime requested but {} is unavailable",
         VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  if (ContainsLayer(debug_runtime->available_instance_layers,
                    "VK_LAYER_KHRONOS_validation")) {
    TryEnableInstanceLayer(&debug_runtime->enabled_instance_layers,
                           debug_runtime->available_instance_layers,
                           "VK_LAYER_KHRONOS_validation");
  } else {
    LOGW(
        "Vulkan debug runtime requested but VK_LAYER_KHRONOS_validation is "
        "unavailable");
  }
}

void ApplyDebugRuntimeToInstanceCreateInfo(
    const VulkanDebugRuntimeState& debug_runtime,
    std::vector<const char*>* enabled_layer_ptrs,
    VkInstanceCreateInfo* instance_info) {
  if (enabled_layer_ptrs == nullptr || instance_info == nullptr) {
    return;
  }

  *enabled_layer_ptrs = BuildNamePtrs(debug_runtime.enabled_instance_layers);
  instance_info->enabledLayerCount =
      static_cast<uint32_t>(enabled_layer_ptrs->size());
  instance_info->ppEnabledLayerNames = enabled_layer_ptrs->data();
}

void MarkDebugRuntimeKnownAndLog(VulkanDebugRuntimeState* debug_runtime) {
  if (debug_runtime == nullptr) {
    return;
  }
  debug_runtime->enabled_instance_layers_known = true;
  LogEnabledLayers(debug_runtime->enabled_instance_layers);
}

VkBool32 VKAPI_CALL VulkanDebugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  (void)user_data;

  const char* message =
      callback_data != nullptr && callback_data->pMessage != nullptr
          ? callback_data->pMessage
          : "Unknown Vulkan validation message";

  const bool is_validation =
      (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0;
  const bool is_performance =
      (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0;

  if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
    LOGE("Vulkan validation{}{}: {}", is_validation ? "" : " message",
         is_performance ? " [performance]" : "", message);
  } else if ((message_severity &
              VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
    LOGW("Vulkan validation{}{}: {}", is_validation ? "" : " message",
         is_performance ? " [performance]" : "", message);
  } else {
    LOGD("Vulkan validation{}{}: {}", is_validation ? "" : " message",
         is_performance ? " [performance]" : "", message);
  }

  return VK_FALSE;
}

void TryCreateDebugUtilsMessenger(VkInstance instance,
                                  const VulkanInstanceFns& instance_fns,
                                  VulkanDebugRuntimeState* debug_runtime) {
  if (instance == VK_NULL_HANDLE || debug_runtime == nullptr ||
      debug_runtime->debug_utils_messenger != VK_NULL_HANDLE ||
      instance_fns.vkCreateDebugUtilsMessengerEXT == nullptr) {
    return;
  }

  VkDebugUtilsMessengerCreateInfoEXT create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = VulkanDebugUtilsMessengerCallback;

  const VkResult result = instance_fns.vkCreateDebugUtilsMessengerEXT(
      instance, &create_info, nullptr, &debug_runtime->debug_utils_messenger);
  if (result != VK_SUCCESS) {
    LOGW("Failed to create Vulkan debug utils messenger: {}",
         static_cast<int32_t>(result));
    debug_runtime->debug_utils_messenger = VK_NULL_HANDLE;
    return;
  }

  LOGD("Created Vulkan debug utils messenger: {:p}",
       reinterpret_cast<void*>(debug_runtime->debug_utils_messenger));
}

}  // namespace

#else

void HandleUserProvidedDebugRuntimeHint(const GPUContextInfoVK& info) {
  (void)info;
}

void ConfigureDebugRuntimeForOwnedInstance(
    const GPUContextInfoVK& info,
    const std::vector<VkExtensionProperties>& available_instance_extensions,
    std::vector<std::string>* enabled_instance_extensions,
    VulkanDebugRuntimeState* debug_runtime) {
  (void)info;
  (void)available_instance_extensions;
  (void)enabled_instance_extensions;
  (void)debug_runtime;
}

void ApplyDebugRuntimeToInstanceCreateInfo(
    const VulkanDebugRuntimeState& debug_runtime,
    std::vector<const char*>* enabled_layer_ptrs,
    VkInstanceCreateInfo* instance_info) {
  (void)debug_runtime;
  (void)enabled_layer_ptrs;
  (void)instance_info;
}

void MarkDebugRuntimeKnownAndLog(VulkanDebugRuntimeState* debug_runtime) {
  (void)debug_runtime;
}

}  // namespace

#endif

VulkanContextState::~VulkanContextState() { Reset(); }

bool VulkanContextState::AreEnabledInstanceLayersKnown() const {
#if !defined(SKITY_VK_DEBUG_RUNTIME)
  return false;
#else
  return debug_runtime_.enabled_instance_layers_known;
#endif
}

const std::vector<VkLayerProperties>&
VulkanContextState::GetAvailableInstanceLayers() const {
#if !defined(SKITY_VK_DEBUG_RUNTIME)
  static const std::vector<VkLayerProperties> kEmptyLayers = {};
  return kEmptyLayers;
#else
  return debug_runtime_.available_instance_layers;
#endif
}

const std::vector<std::string>& VulkanContextState::GetEnabledInstanceLayers()
    const {
#if !defined(SKITY_VK_DEBUG_RUNTIME)
  static const std::vector<std::string> kEmptyLayers = {};
  return kEmptyLayers;
#else
  return debug_runtime_.enabled_instance_layers;
#endif
}

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

void VulkanContextState::EnqueuePendingSubmission(
    VulkanPendingSubmission submission) const {
  pending_submissions_.emplace_back(std::move(submission));
}

void VulkanContextState::CollectPendingSubmissions(bool wait_all) const {
  if (logical_device_ == VK_NULL_HANDLE ||
      functions_.device.vkDestroyFence == nullptr ||
      functions_.device.vkDestroyCommandPool == nullptr) {
    return;
  }

  std::vector<VulkanPendingSubmission> completed_submissions;
  if (pending_submissions_.empty()) {
    return;
  }

  if (wait_all) {
    std::vector<VkFence> fences;
    fences.reserve(pending_submissions_.size());
    for (const auto& submission : pending_submissions_) {
      if (submission.fence != VK_NULL_HANDLE) {
        fences.push_back(submission.fence);
      }
    }

    if (!fences.empty()) {
      const VkResult result = functions_.device.vkWaitForFences(
          logical_device_, static_cast<uint32_t>(fences.size()), fences.data(),
          VK_TRUE, UINT64_MAX);
      if (result != VK_SUCCESS) {
        LOGW("Failed to wait Vulkan pending submissions: {}",
             static_cast<int32_t>(result));
      }
    }

    completed_submissions = std::move(pending_submissions_);
    pending_submissions_.clear();
  } else {
    auto it = pending_submissions_.begin();
    while (it != pending_submissions_.end()) {
      const VkResult result =
          functions_.device.vkGetFenceStatus(logical_device_, it->fence);
      if (result == VK_SUCCESS) {
        completed_submissions.emplace_back(std::move(*it));
        it = pending_submissions_.erase(it);
        continue;
      }

      if (result != VK_NOT_READY) {
        LOGW("Failed to query Vulkan fence status: {}",
             static_cast<int32_t>(result));
      }
      ++it;
    }
  }

  for (auto& submission : completed_submissions) {
    if (submission.owns_fence && submission.fence != VK_NULL_HANDLE) {
      functions_.device.vkDestroyFence(logical_device_, submission.fence,
                                       nullptr);
      submission.fence = VK_NULL_HANDLE;
    }

    for (auto& cleanup_action : submission.cleanup_actions) {
      if (cleanup_action) {
        cleanup_action();
      }
    }
    submission.cleanup_actions.clear();

    if (submission.command_pool != VK_NULL_HANDLE) {
      functions_.device.vkDestroyCommandPool(logical_device_,
                                             submission.command_pool, nullptr);
      submission.command_pool = VK_NULL_HANDLE;
    }
  }
}

VkRenderPass VulkanContextState::GetOrCreateLegacyRenderPass(
    const LegacyRenderPassKey& key) const {
  return render_pass_cache_.GetOrCreate(*this, key);
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

  if (!LoadAvailableInstanceLayers()) {
    return false;
  }

  api_version_ = ResolveInstanceApiVersion(functions_.global);

  if (info.instance != VK_NULL_HANDLE) {
    instance_ = info.instance;
    LOGD("Using user provided Vulkan instance: {:p}",
         reinterpret_cast<void*>(instance_));
    HandleUserProvidedDebugRuntimeHint(info);
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
    EnablePortabilityEnumerationIfAvailable(available_instance_extensions_,
                                            &enabled_instance_extensions_,
                                            &instance_flags);

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

    ConfigureDebugRuntimeForOwnedInstance(info, available_instance_extensions_,
                                          &enabled_instance_extensions_,
                                          &debug_runtime_);

    auto enabled_extension_ptrs = BuildNamePtrs(enabled_instance_extensions_);
    std::vector<const char*> enabled_layer_ptrs;

    const uint32_t target_version = api_version_;
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
    ApplyDebugRuntimeToInstanceCreateInfo(debug_runtime_, &enabled_layer_ptrs,
                                          &instance_info);
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
    if (HasEnabledInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
      TryCreateDebugUtilsMessenger(instance_, functions_.instance,
                                   &debug_runtime_);
    }
    MarkDebugRuntimeKnownAndLog(&debug_runtime_);
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
    api_version_ = MinApiVersion(api_version_, properties.apiVersion);
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
    api_version_ = MinApiVersion(api_version_, selected_api_version);
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

bool VulkanContextState::LoadAvailableInstanceLayers() {
  return LoadAvailableInstanceLayersImpl(functions_.global, &debug_runtime_);
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
  synchronization2_enabled_ = false;
  dynamic_rendering_enabled_ = false;
  pipeline_cache_ = VK_NULL_HANDLE;

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
      synchronization2_enabled_ = ContainsExtension(
          enabled_device_extensions_, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
      dynamic_rendering_enabled_ = ContainsExtension(
          enabled_device_extensions_, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
      if (dynamic_rendering_enabled_ &&
          (functions_.device.vkCmdBeginRendering == nullptr ||
           functions_.device.vkCmdEndRendering == nullptr)) {
        LOGW(
            "Dynamic rendering was declared enabled, but begin/end rendering "
            "procedures are unavailable; falling back to legacy render pass");
        dynamic_rendering_enabled_ = false;
      }
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
    const bool has_dynamic_rendering_extension =
        HasAvailableDeviceExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    if (has_dynamic_rendering_extension) {
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
    if (HasAvailableDeviceExtension(kPortabilitySubsetExtensionName)) {
      enabled_device_extensions_.emplace_back(kPortabilitySubsetExtensionName);
    }

    VkPhysicalDeviceFeatures2 physical_device_features = {};
    physical_device_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features = {};
    dynamic_rendering_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    VkPhysicalDeviceSynchronization2Features synchronization2_features = {};
    synchronization2_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;

    void** feature_query_next = &physical_device_features.pNext;
    if (has_dynamic_rendering_extension) {
      *feature_query_next = &dynamic_rendering_features;
      feature_query_next = &dynamic_rendering_features.pNext;
    }
    if (HasAvailableDeviceExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
      *feature_query_next = &synchronization2_features;
      feature_query_next = &synchronization2_features.pNext;
    }

    if (physical_device_features.pNext != nullptr) {
      functions_.instance.vkGetPhysicalDeviceFeatures2(
          physical_device_, &physical_device_features);
    }

    if (has_dynamic_rendering_extension) {
      if (dynamic_rendering_features.dynamicRendering == VK_TRUE) {
        dynamic_rendering_features.dynamicRendering = VK_TRUE;
        dynamic_rendering_enabled_ = true;
      } else {
        enabled_device_extensions_.erase(
            std::remove(enabled_device_extensions_.begin(),
                        enabled_device_extensions_.end(),
                        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME),
            enabled_device_extensions_.end());
        LOGW(
            "Vulkan dynamic rendering extension is present but feature is "
            "not supported, disabling extension request");
      }
    }

    if (HasAvailableDeviceExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
      if (synchronization2_features.synchronization2 == VK_TRUE) {
        synchronization2_features.synchronization2 = VK_TRUE;
        synchronization2_enabled_ = true;
      } else {
        enabled_device_extensions_.erase(
            std::remove(enabled_device_extensions_.begin(),
                        enabled_device_extensions_.end(),
                        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME),
            enabled_device_extensions_.end());
        LOGW(
            "Vulkan synchronization2 extension is present but feature is not "
            "supported, disabling extension request");
      }
    }

    auto enabled_extension_ptrs = BuildNamePtrs(enabled_device_extensions_);

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

    void* enabled_feature_chain = nullptr;
    void** enabled_feature_next = &enabled_feature_chain;
    if (dynamic_rendering_enabled_) {
      *enabled_feature_next = &dynamic_rendering_features;
      enabled_feature_next = &dynamic_rendering_features.pNext;
    }
    if (synchronization2_enabled_) {
      *enabled_feature_next = &synchronization2_features;
      enabled_feature_next = &synchronization2_features.pNext;
    }
    if (enabled_feature_chain != nullptr) {
      device_info.pNext = enabled_feature_chain;
    }
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
    if (dynamic_rendering_enabled_ &&
        (functions_.device.vkCmdBeginRendering == nullptr ||
         functions_.device.vkCmdEndRendering == nullptr)) {
      LOGW(
          "Dynamic rendering feature was enabled, but begin/end rendering "
          "procedures are unavailable; falling back to legacy render pass");
      dynamic_rendering_enabled_ = false;
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

  VkPipelineCacheCreateInfo pipeline_cache_info = {};
  pipeline_cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  const VkResult pipeline_cache_result =
      functions_.device.vkCreatePipelineCache(
          logical_device_, &pipeline_cache_info, nullptr, &pipeline_cache_);
  if (pipeline_cache_result != VK_SUCCESS ||
      pipeline_cache_ == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan pipeline cache: result={}",
         static_cast<int32_t>(pipeline_cache_result));
    pipeline_cache_ = VK_NULL_HANDLE;
    return false;
  }

  LOGD("Created Vulkan pipeline cache: {:p}",
       reinterpret_cast<void*>(pipeline_cache_));

  VmaVulkanFunctions vulkan_functions = {};
  vulkan_functions.vkGetInstanceProcAddr = functions_.get_instance_proc_addr;
  vulkan_functions.vkGetDeviceProcAddr = functions_.get_device_proc_addr;
  vulkan_functions.vkGetPhysicalDeviceProperties =
      functions_.instance.vkGetPhysicalDeviceProperties;
  vulkan_functions.vkGetPhysicalDeviceMemoryProperties =
      functions_.instance.vkGetPhysicalDeviceMemoryProperties;
  vulkan_functions.vkAllocateMemory = functions_.device.vkAllocateMemory;
  vulkan_functions.vkFreeMemory = functions_.device.vkFreeMemory;
  vulkan_functions.vkMapMemory = functions_.device.vkMapMemory;
  vulkan_functions.vkUnmapMemory = functions_.device.vkUnmapMemory;
  vulkan_functions.vkFlushMappedMemoryRanges =
      functions_.device.vkFlushMappedMemoryRanges;
  vulkan_functions.vkInvalidateMappedMemoryRanges =
      functions_.device.vkInvalidateMappedMemoryRanges;
  vulkan_functions.vkBindBufferMemory = functions_.device.vkBindBufferMemory;
  vulkan_functions.vkBindImageMemory = functions_.device.vkBindImageMemory;
  vulkan_functions.vkGetBufferMemoryRequirements =
      functions_.device.vkGetBufferMemoryRequirements;
  vulkan_functions.vkGetImageMemoryRequirements =
      functions_.device.vkGetImageMemoryRequirements;
  vulkan_functions.vkCreateBuffer = functions_.device.vkCreateBuffer;
  vulkan_functions.vkDestroyBuffer = functions_.device.vkDestroyBuffer;
  vulkan_functions.vkCreateImage = functions_.device.vkCreateImage;
  vulkan_functions.vkDestroyImage = functions_.device.vkDestroyImage;
  vulkan_functions.vkCmdCopyBuffer = functions_.device.vkCmdCopyBuffer;
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
  vulkan_functions.vkGetBufferMemoryRequirements2KHR =
      functions_.device.vkGetBufferMemoryRequirements2KHR;
  vulkan_functions.vkGetImageMemoryRequirements2KHR =
      functions_.device.vkGetImageMemoryRequirements2KHR;
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
  vulkan_functions.vkBindBufferMemory2KHR =
      functions_.device.vkBindBufferMemory2KHR;
  vulkan_functions.vkBindImageMemory2KHR =
      functions_.device.vkBindImageMemory2KHR;
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
  vulkan_functions.vkGetPhysicalDeviceMemoryProperties2KHR =
      functions_.instance.vkGetPhysicalDeviceMemoryProperties2KHR;
#endif
#if VMA_KHR_MAINTENANCE4 || VMA_VULKAN_VERSION >= 1003000
  vulkan_functions.vkGetDeviceBufferMemoryRequirements =
      functions_.device.vkGetDeviceBufferMemoryRequirements;
  vulkan_functions.vkGetDeviceImageMemoryRequirements =
      functions_.device.vkGetDeviceImageMemoryRequirements;
#endif

  VmaAllocatorCreateInfo allocator_info = {};
  allocator_info.instance = instance_;
  allocator_info.physicalDevice = physical_device_;
  allocator_info.device = logical_device_;
  allocator_info.pVulkanFunctions = &vulkan_functions;
  allocator_info.vulkanApiVersion = api_version_;

  const VkResult allocator_result =
      vmaCreateAllocator(&allocator_info, &allocator_);
  if (allocator_result != VK_SUCCESS || allocator_ == nullptr) {
    LOGE("Failed to create VMA allocator: result={}",
         static_cast<int32_t>(allocator_result));
    allocator_ = nullptr;
    return false;
  }

  LOGD("Created VMA allocator: {:p}", reinterpret_cast<void*>(allocator_));

  return graphics_queue_ != VK_NULL_HANDLE;
}

void VulkanContextState::Reset() {
#if defined(SKITY_VK_DEBUG_RUNTIME)
  if (debug_runtime_.debug_utils_messenger != VK_NULL_HANDLE &&
      instance_ != VK_NULL_HANDLE &&
      functions_.instance.vkDestroyDebugUtilsMessengerEXT != nullptr) {
    LOGD("Destroying Vulkan debug utils messenger: {:p}",
         reinterpret_cast<void*>(debug_runtime_.debug_utils_messenger));
    functions_.instance.vkDestroyDebugUtilsMessengerEXT(
        instance_, debug_runtime_.debug_utils_messenger, nullptr);
    debug_runtime_.debug_utils_messenger = VK_NULL_HANDLE;
  }
#endif

  CollectPendingSubmissions(true);

  render_pass_cache_.Reset(*this);

  if (pipeline_cache_ != VK_NULL_HANDLE && logical_device_ != VK_NULL_HANDLE &&
      functions_.device.vkDestroyPipelineCache != nullptr) {
    LOGD("Destroying Vulkan pipeline cache: {:p}",
         reinterpret_cast<void*>(pipeline_cache_));
    functions_.device.vkDestroyPipelineCache(logical_device_, pipeline_cache_,
                                             nullptr);
    pipeline_cache_ = VK_NULL_HANDLE;
  }

  if (allocator_ != nullptr) {
    LOGD("Destroying VMA allocator: {:p}", reinterpret_cast<void*>(allocator_));
    vmaDestroyAllocator(allocator_);
    allocator_ = nullptr;
  }

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
  synchronization2_enabled_ = false;
  dynamic_rendering_enabled_ = false;
  debug_runtime_ = {};
  functions_ = {};
  instance_ = VK_NULL_HANDLE;
  physical_device_ = VK_NULL_HANDLE;
  logical_device_ = VK_NULL_HANDLE;
  graphics_queue_ = VK_NULL_HANDLE;
  compute_queue_ = VK_NULL_HANDLE;
  transfer_queue_ = VK_NULL_HANDLE;
  pipeline_cache_ = VK_NULL_HANDLE;
  allocator_ = nullptr;
  api_version_ = VK_API_VERSION_1_0;
  graphics_queue_family_index_ = -1;
  compute_queue_family_index_ = -1;
  transfer_queue_family_index_ = -1;
  own_instance_ = false;
  own_device_ = false;
}

}  // namespace skity
