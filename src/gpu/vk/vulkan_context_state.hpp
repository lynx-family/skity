// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_VULKAN_CONTEXT_STATE_HPP
#define SRC_GPU_VK_VULKAN_CONTEXT_STATE_HPP

#include <vk_mem_alloc.h>

#include <skity/gpu/gpu_context_vk.hpp>
#include <string>
#include <vector>

#include "src/gpu/vk/vulkan_proc_table.hpp"

namespace skity {

#if defined(SKITY_VK_DEBUG_RUNTIME)
struct VulkanDebugRuntimeState {
  std::vector<VkLayerProperties> available_instance_layers = {};
  std::vector<std::string> enabled_instance_layers = {};
  bool enabled_instance_layers_known = false;
  VkDebugUtilsMessengerEXT debug_utils_messenger = VK_NULL_HANDLE;
};
#else
struct VulkanDebugRuntimeState {};
#endif

class VulkanContextState {
 public:
  VulkanContextState() = default;

  ~VulkanContextState();

  bool Initialize(const GPUContextInfoVK& info);

  VkInstance GetInstance() const { return instance_; }

  VkPhysicalDevice GetPhysicalDevice() const { return physical_device_; }

  VkDevice GetLogicalDevice() const { return logical_device_; }

  VkQueue GetGraphicsQueue() const { return graphics_queue_; }

  VmaAllocator GetAllocator() const { return allocator_; }

  const VulkanFunctionPointers& Fns() const { return functions_; }

  const VulkanGlobalFns& GlobalFns() const { return functions_.global; }

  const VulkanInstanceFns& InstanceFns() const { return functions_.instance; }

  const VulkanDeviceFns& DeviceFns() const { return functions_.device; }

  PFN_vkGetInstanceProcAddr GetInstanceProcAddr() const {
    return functions_.get_instance_proc_addr;
  }

  PFN_vkGetDeviceProcAddr GetDeviceProcAddr() const {
    return functions_.get_device_proc_addr;
  }

  bool HasAvailableInstanceExtension(const char* extension_name) const;

  bool HasEnabledInstanceExtension(const char* extension_name) const;

  bool HasAvailableDeviceExtension(const char* extension_name) const;

  bool HasEnabledDeviceExtension(const char* extension_name) const;

  bool AreEnabledInstanceExtensionsKnown() const {
    return enabled_instance_extensions_known_;
  }

  bool AreEnabledInstanceLayersKnown() const;

  bool AreEnabledDeviceExtensionsKnown() const {
    return enabled_device_extensions_known_;
  }

  const std::vector<VkExtensionProperties>& GetAvailableInstanceExtensions()
      const {
    return available_instance_extensions_;
  }

  const std::vector<VkExtensionProperties>& GetAvailableDeviceExtensions()
      const {
    return available_device_extensions_;
  }

  const std::vector<VkLayerProperties>& GetAvailableInstanceLayers() const;

  const std::vector<std::string>& GetEnabledInstanceExtensions() const {
    return enabled_instance_extensions_;
  }

  const std::vector<std::string>& GetEnabledInstanceLayers() const;

  const std::vector<std::string>& GetEnabledDeviceExtensions() const {
    return enabled_device_extensions_;
  }

 private:
  int32_t FindQueueFamilyIndex(VkQueueFlags flags, bool prefer_dedicated) const;
  bool InitializeInstance(const GPUContextInfoVK& info);
  bool InitializePhysicalDevice(const GPUContextInfoVK& info);
  bool InitializeQueues(const GPUContextInfoVK& info);
  bool InitializeDevice(const GPUContextInfoVK& info);
  bool LoadAvailableInstanceExtensions();
  bool LoadAvailableInstanceLayers();
  bool LoadAvailableDeviceExtensions();
  bool LoadDeviceFns();
  void Reset();

  VulkanFunctionPointers functions_ = {};
  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice logical_device_ = VK_NULL_HANDLE;
  VkQueue graphics_queue_ = VK_NULL_HANDLE;
  VkQueue compute_queue_ = VK_NULL_HANDLE;
  VkQueue transfer_queue_ = VK_NULL_HANDLE;
  VmaAllocator allocator_ = nullptr;
  uint32_t api_version_ = VK_API_VERSION_1_0;
  int32_t graphics_queue_family_index_ = -1;
  int32_t compute_queue_family_index_ = -1;
  int32_t transfer_queue_family_index_ = -1;
  bool own_instance_ = false;
  bool own_device_ = false;
  std::vector<VkQueueFamilyProperties> queue_family_properties_ = {};
  std::vector<VkExtensionProperties> available_instance_extensions_ = {};
  std::vector<VkExtensionProperties> available_device_extensions_ = {};
  std::vector<std::string> enabled_instance_extensions_ = {};
  std::vector<std::string> enabled_device_extensions_ = {};
  bool enabled_instance_extensions_known_ = false;
  bool enabled_device_extensions_known_ = false;
  VulkanDebugRuntimeState debug_runtime_ = {};
};

}  // namespace skity

#endif  // SRC_GPU_VK_VULKAN_CONTEXT_STATE_HPP
