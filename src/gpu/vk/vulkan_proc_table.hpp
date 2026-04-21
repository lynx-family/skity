// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_VULKAN_PROC_TABLE_HPP
#define SRC_GPU_VK_VULKAN_PROC_TABLE_HPP

#include <vulkan/vulkan.h>

namespace skity {

struct VulkanGlobalFns {
  PFN_vkCreateInstance vkCreateInstance = nullptr;
  PFN_vkEnumerateInstanceExtensionProperties
      vkEnumerateInstanceExtensionProperties = nullptr;
  PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties =
      nullptr;
  PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion = nullptr;
};

struct VulkanInstanceFns {
  PFN_vkDestroyInstance vkDestroyInstance = nullptr;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
  PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
  PFN_vkEnumerateDeviceExtensionProperties
      vkEnumerateDeviceExtensionProperties = nullptr;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties
      vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
  PFN_vkCreateDevice vkCreateDevice = nullptr;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
#if defined(SKITY_VK_DEBUG_RUNTIME)
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
#endif
};

struct VulkanDeviceFns {
  PFN_vkDestroyDevice vkDestroyDevice = nullptr;
  PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
  PFN_vkQueueSubmit vkQueueSubmit = nullptr;
  PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
  PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
  PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
  PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
  PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
  PFN_vkCreateFence vkCreateFence = nullptr;
  PFN_vkDestroyFence vkDestroyFence = nullptr;
  PFN_vkGetFenceStatus vkGetFenceStatus = nullptr;
  PFN_vkWaitForFences vkWaitForFences = nullptr;
  PFN_vkCmdCopyBuffer vkCmdCopyBuffer = nullptr;
  PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
  PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
  PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = nullptr;
#if defined(SKITY_VK_DEBUG_RUNTIME)
  PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT = nullptr;
  PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT = nullptr;
  PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT = nullptr;
#endif
};

struct VulkanFunctionPointers {
  PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;
  PFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;
  VulkanGlobalFns global = {};
  VulkanInstanceFns instance = {};
  VulkanDeviceFns device = {};
};

struct GPUContextInfoVK;

bool LoadVulkanGlobalFns(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                         VulkanGlobalFns* fns);

bool LoadVulkanInstanceFns(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                           VkInstance instance, VulkanInstanceFns* fns);

bool LoadVulkanDeviceFns(PFN_vkGetDeviceProcAddr get_device_proc_addr,
                         VkDevice device, VulkanDeviceFns* fns);

bool CreateVkInstance(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                      VkInstance* instance,
                      VulkanFunctionPointers* functions = nullptr,
                      const VkInstanceCreateInfo* create_info = nullptr,
                      const VkAllocationCallbacks* allocator = nullptr);

}  // namespace skity

#endif  // SRC_GPU_VK_VULKAN_PROC_TABLE_HPP
