// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_VULKAN_CONTEXT_STATE_HPP
#define SRC_GPU_VK_VULKAN_CONTEXT_STATE_HPP

#include <vk_mem_alloc.h>

#include <skity/gpu/gpu_context_vk.hpp>
#include <vector>

#include "src/gpu/vk/vulkan_debug_runtime_state.hpp"
#include "src/gpu/vk/vulkan_pending_submission.hpp"
#include "src/gpu/vk/vulkan_proc_table.hpp"
#include "src/gpu/vk/vulkan_render_pass_cache.hpp"

namespace skity {

class VulkanContextState {
 public:
  using LegacyRenderPassKey = VulkanRenderPassCache::Key;

  VulkanContextState() = default;

  ~VulkanContextState();

  bool Initialize(const GPUContextInfoVK& info);

  VkInstance GetInstance() const { return instance_; }

  VkPhysicalDevice GetPhysicalDevice() const { return physical_device_; }

  VkDevice GetLogicalDevice() const { return logical_device_; }

  VkQueue GetGraphicsQueue() const { return graphics_queue_; }

  VkPipelineCache GetPipelineCache() const { return pipeline_cache_; }

  int32_t GetGraphicsQueueFamilyIndex() const {
    return graphics_queue_family_index_;
  }

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

  bool IsSynchronization2Enabled() const { return synchronization2_enabled_; }

  bool IsDynamicRenderingEnabled() const { return dynamic_rendering_enabled_; }

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

  void EnqueuePendingSubmission(VulkanPendingSubmission submission) const;

  void CollectPendingSubmissions(bool wait_all) const;

  VkRenderPass GetOrCreateLegacyRenderPass(
      const LegacyRenderPassKey& key) const;

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
  VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
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
  bool synchronization2_enabled_ = false;
  bool dynamic_rendering_enabled_ = false;
  VulkanDebugRuntimeState debug_runtime_ = {};
  mutable std::vector<VulkanPendingSubmission> pending_submissions_ = {};
  mutable VulkanRenderPassCache render_pass_cache_ = {};
};

}  // namespace skity

#endif  // SRC_GPU_VK_VULKAN_CONTEXT_STATE_HPP
