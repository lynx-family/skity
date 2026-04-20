// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <vulkan/vulkan.h>

#include <skity/gpu/gpu_context_vk.hpp>
#include <string>
#include <vector>

#include "src/gpu/vk/gpu_context_impl_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/gpu/vk/vulkan_proc_table.hpp"

namespace {

int32_t FindGraphicsQueueFamilyIndex(
    PFN_vkGetPhysicalDeviceQueueFamilyProperties
        vk_get_physical_device_queue_family_properties,
    VkPhysicalDevice physical_device) {
  uint32_t queue_family_count = 0;
  vk_get_physical_device_queue_family_properties(physical_device,
                                                 &queue_family_count, nullptr);
  if (queue_family_count == 0) {
    return -1;
  }

  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vk_get_physical_device_queue_family_properties(
      physical_device, &queue_family_count, queue_families.data());

  for (uint32_t i = 0; i < queue_family_count; ++i) {
    if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
        queue_families[i].queueCount > 0) {
      return static_cast<int32_t>(i);
    }
  }

  return -1;
}

TEST(VulkanProcLoaderTest, CreateVkInstanceWithProcLoader) {
  skity::VulkanFunctionPointers functions = {};
  VkInstance instance = VK_NULL_HANDLE;

  ASSERT_TRUE(
      skity::CreateVkInstance(vkGetInstanceProcAddr, &instance, &functions));
  ASSERT_NE(instance, VK_NULL_HANDLE);
  EXPECT_NE(functions.global.vkCreateInstance, nullptr);
  EXPECT_NE(functions.instance.vkDestroyInstance, nullptr);
  EXPECT_NE(functions.instance.vkEnumeratePhysicalDevices, nullptr);
  EXPECT_NE(functions.instance.vkGetDeviceProcAddr, nullptr);

  functions.instance.vkDestroyInstance(instance, nullptr);
}

TEST(VulkanProcLoaderTest, CreateDeviceAndLoadDeviceFns) {
  skity::VulkanFunctionPointers functions = {};
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;

  ASSERT_TRUE(
      skity::CreateVkInstance(vkGetInstanceProcAddr, &instance, &functions));
  ASSERT_NE(instance, VK_NULL_HANDLE);

  uint32_t physical_device_count = 0;
  ASSERT_EQ(functions.instance.vkEnumeratePhysicalDevices(
                instance, &physical_device_count, nullptr),
            VK_SUCCESS);
  ASSERT_GT(physical_device_count, 0u);

  std::vector<VkPhysicalDevice> physical_devices(physical_device_count,
                                                 VK_NULL_HANDLE);
  ASSERT_EQ(functions.instance.vkEnumeratePhysicalDevices(
                instance, &physical_device_count, physical_devices.data()),
            VK_SUCCESS);

  const VkPhysicalDevice physical_device = physical_devices[0];
  ASSERT_NE(physical_device, VK_NULL_HANDLE);

  const int32_t graphics_queue_family_index = FindGraphicsQueueFamilyIndex(
      functions.instance.vkGetPhysicalDeviceQueueFamilyProperties,
      physical_device);
  ASSERT_GE(graphics_queue_family_index, 0);

  const float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_info = {};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex =
      static_cast<uint32_t>(graphics_queue_family_index);
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &queue_priority;

  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;

  ASSERT_EQ(functions.instance.vkCreateDevice(physical_device, &device_info,
                                              nullptr, &device),
            VK_SUCCESS);
  ASSERT_NE(device, VK_NULL_HANDLE);

  ASSERT_TRUE(skity::LoadVulkanDeviceFns(functions.instance.vkGetDeviceProcAddr,
                                         device, &functions.device));
  EXPECT_NE(functions.device.vkDestroyDevice, nullptr);
  EXPECT_NE(functions.device.vkGetDeviceQueue, nullptr);

  VkQueue graphics_queue = VK_NULL_HANDLE;
  functions.device.vkGetDeviceQueue(
      device, static_cast<uint32_t>(graphics_queue_family_index), 0,
      &graphics_queue);
  EXPECT_NE(graphics_queue, VK_NULL_HANDLE);

  functions.device.vkDestroyDevice(device, nullptr);
  functions.instance.vkDestroyInstance(instance, nullptr);
}

TEST(VulkanProcLoaderTest, CreateGPUContextWithProcLoader) {
  auto context = skity::CreateGPUContextVK(vkGetInstanceProcAddr);

  ASSERT_NE(context, nullptr);
  EXPECT_EQ(context->GetBackendType(), skity::GPUBackendType::kVulkan);

  auto* vk_context = static_cast<skity::GPUContextVK*>(context.get());
  ASSERT_NE(vk_context, nullptr);
  ASSERT_NE(vk_context->GetState(), nullptr);
  EXPECT_NE(vk_context->GetState()->GetInstance(), VK_NULL_HANDLE);
  EXPECT_NE(vk_context->GetState()->GetPhysicalDevice(), VK_NULL_HANDLE);
  EXPECT_NE(vk_context->GetState()->GetLogicalDevice(), VK_NULL_HANDLE);
  EXPECT_NE(vk_context->GetState()->GetGraphicsQueue(), VK_NULL_HANDLE);
}

TEST(VulkanProcLoaderTest, CreateGPUContextWithInfo) {
  skity::GPUContextInfoVK info = {};
  info.get_instance_proc_addr = vkGetInstanceProcAddr;

  auto context = skity::CreateGPUContextVK(&info);

  ASSERT_NE(context, nullptr);
  EXPECT_EQ(context->GetBackendType(), skity::GPUBackendType::kVulkan);

  auto* vk_context = static_cast<skity::GPUContextVK*>(context.get());
  ASSERT_NE(vk_context, nullptr);
  ASSERT_NE(vk_context->GetState(), nullptr);
  EXPECT_TRUE(vk_context->GetState()->AreEnabledInstanceExtensionsKnown());
  EXPECT_TRUE(vk_context->GetState()->AreEnabledDeviceExtensionsKnown());
  EXPECT_EQ(
      vk_context->GetState()->HasEnabledInstanceExtension("VK_KHR_surface"),
      vk_context->GetState()->HasAvailableInstanceExtension("VK_KHR_surface"));
  EXPECT_EQ(
      vk_context->GetState()->HasEnabledDeviceExtension("VK_KHR_swapchain"),
      vk_context->GetState()->HasAvailableDeviceExtension("VK_KHR_swapchain"));
}

TEST(VulkanProcLoaderTest, PreserveUserProvidedExtensionInfo) {
  skity::VulkanFunctionPointers functions = {};
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  skity::VulkanDeviceFns device_fns = {};

  ASSERT_TRUE(
      skity::CreateVkInstance(vkGetInstanceProcAddr, &instance, &functions));
  ASSERT_NE(instance, VK_NULL_HANDLE);

  uint32_t physical_device_count = 0;
  ASSERT_EQ(functions.instance.vkEnumeratePhysicalDevices(
                instance, &physical_device_count, nullptr),
            VK_SUCCESS);
  ASSERT_GT(physical_device_count, 0u);

  std::vector<VkPhysicalDevice> physical_devices(physical_device_count,
                                                 VK_NULL_HANDLE);
  ASSERT_EQ(functions.instance.vkEnumeratePhysicalDevices(
                instance, &physical_device_count, physical_devices.data()),
            VK_SUCCESS);

  const VkPhysicalDevice physical_device = physical_devices[0];
  ASSERT_NE(physical_device, VK_NULL_HANDLE);

  const int32_t graphics_queue_family_index = FindGraphicsQueueFamilyIndex(
      functions.instance.vkGetPhysicalDeviceQueueFamilyProperties,
      physical_device);
  ASSERT_GE(graphics_queue_family_index, 0);

  std::vector<const char*> enabled_device_extensions;
  if (functions.instance.vkEnumerateDeviceExtensionProperties != nullptr) {
    uint32_t extension_count = 0;
    ASSERT_EQ(functions.instance.vkEnumerateDeviceExtensionProperties(
                  physical_device, nullptr, &extension_count, nullptr),
              VK_SUCCESS);
    if (extension_count > 0) {
      std::vector<VkExtensionProperties> extensions(extension_count);
      ASSERT_EQ(
          functions.instance.vkEnumerateDeviceExtensionProperties(
              physical_device, nullptr, &extension_count, extensions.data()),
          VK_SUCCESS);
      for (const auto& extension : extensions) {
        if (std::string(extension.extensionName) ==
            VK_KHR_SWAPCHAIN_EXTENSION_NAME) {
          enabled_device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
          break;
        }
      }
    }
  }

  const float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_info = {};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex =
      static_cast<uint32_t>(graphics_queue_family_index);
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &queue_priority;

  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  device_info.enabledExtensionCount =
      static_cast<uint32_t>(enabled_device_extensions.size());
  device_info.ppEnabledExtensionNames = enabled_device_extensions.data();

  ASSERT_EQ(functions.instance.vkCreateDevice(physical_device, &device_info,
                                              nullptr, &device),
            VK_SUCCESS);
  ASSERT_NE(device, VK_NULL_HANDLE);
  ASSERT_TRUE(skity::LoadVulkanDeviceFns(functions.instance.vkGetDeviceProcAddr,
                                         device, &device_fns));

  const char* enabled_instance_extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME};

  skity::GPUContextInfoVK info = {};
  info.instance = instance;
  info.get_instance_proc_addr = vkGetInstanceProcAddr;
  info.enabled_instance_extensions = enabled_instance_extensions;
  info.enabled_instance_extension_count = 1;
  info.enabled_instance_extensions_known = true;
  info.physical_device = physical_device;
  info.logical_device = device;
  info.get_device_proc_addr = functions.instance.vkGetDeviceProcAddr;
  info.enabled_device_extensions = enabled_device_extensions.data();
  info.enabled_device_extension_count =
      static_cast<uint32_t>(enabled_device_extensions.size());
  info.enabled_device_extensions_known = true;
  info.graphics_queue_family_index = graphics_queue_family_index;

  auto context = skity::CreateGPUContextVK(&info);

  ASSERT_NE(context, nullptr);
  auto* vk_context = static_cast<skity::GPUContextVK*>(context.get());
  ASSERT_NE(vk_context, nullptr);
  ASSERT_NE(vk_context->GetState(), nullptr);
  EXPECT_TRUE(vk_context->GetState()->AreEnabledInstanceExtensionsKnown());
  EXPECT_TRUE(vk_context->GetState()->HasEnabledInstanceExtension(
      VK_KHR_SURFACE_EXTENSION_NAME));
  EXPECT_TRUE(vk_context->GetState()->AreEnabledDeviceExtensionsKnown());
  EXPECT_EQ(vk_context->GetState()->HasEnabledDeviceExtension(
                VK_KHR_SWAPCHAIN_EXTENSION_NAME),
            !enabled_device_extensions.empty());

  device_fns.vkDestroyDevice(device, nullptr);
  functions.instance.vkDestroyInstance(instance, nullptr);
}

}  // namespace
