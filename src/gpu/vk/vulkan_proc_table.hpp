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
};

struct VulkanDeviceFns {
  PFN_vkDestroyDevice vkDestroyDevice = nullptr;
  PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
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
