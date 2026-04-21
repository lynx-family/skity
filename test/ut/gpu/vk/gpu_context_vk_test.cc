// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <skity/gpu/gpu_context_vk.hpp>
#include <string>
#include <vector>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_shader_module.hpp"
#include "src/gpu/vk/gpu_context_impl_vk.hpp"
#include "src/gpu/vk/gpu_shader_function_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/gpu/vk/vulkan_proc_table.hpp"

namespace {

constexpr char kSimpleVertexWGSL[] = R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)";

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
  const auto* state = vk_context->GetState();
  ASSERT_NE(state, nullptr);
  EXPECT_NE(state->GetInstance(), VK_NULL_HANDLE);
  EXPECT_NE(state->GetPhysicalDevice(), VK_NULL_HANDLE);
  EXPECT_NE(state->GetLogicalDevice(), VK_NULL_HANDLE);
  EXPECT_NE(state->GetGraphicsQueue(), VK_NULL_HANDLE);
  EXPECT_NE(state->GetAllocator(), nullptr);
  EXPECT_NE(state->GetInstanceProcAddr(), nullptr);
  EXPECT_NE(state->GetDeviceProcAddr(), nullptr);
  EXPECT_NE(state->GlobalFns().vkCreateInstance, nullptr);
  EXPECT_NE(state->InstanceFns().vkCreateDevice, nullptr);
  EXPECT_NE(state->DeviceFns().vkGetDeviceQueue, nullptr);
  EXPECT_EQ(state->Fns().device.vkGetDeviceQueue,
            state->DeviceFns().vkGetDeviceQueue);
}

TEST(VulkanProcLoaderTest, CreateGPUContextWithInfo) {
  skity::GPUContextInfoVK info = {};
  info.get_instance_proc_addr = vkGetInstanceProcAddr;

  auto context = skity::CreateGPUContextVK(&info);

  ASSERT_NE(context, nullptr);
  EXPECT_EQ(context->GetBackendType(), skity::GPUBackendType::kVulkan);

  auto* vk_context = static_cast<skity::GPUContextVK*>(context.get());
  ASSERT_NE(vk_context, nullptr);
  const auto* state = vk_context->GetState();
  ASSERT_NE(state, nullptr);
  EXPECT_TRUE(state->AreEnabledInstanceExtensionsKnown());
#if defined(SKITY_VK_DEBUG_RUNTIME)
  EXPECT_TRUE(state->AreEnabledInstanceLayersKnown());
#endif
  EXPECT_TRUE(state->AreEnabledDeviceExtensionsKnown());
  EXPECT_NE(state->GetAllocator(), nullptr);
  EXPECT_EQ(state->HasEnabledInstanceExtension("VK_KHR_surface"),
            state->HasAvailableInstanceExtension("VK_KHR_surface"));
  EXPECT_EQ(state->HasEnabledDeviceExtension("VK_KHR_swapchain"),
            state->HasAvailableDeviceExtension("VK_KHR_swapchain"));
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
  const auto* state = vk_context->GetState();
  ASSERT_NE(state, nullptr);
  EXPECT_TRUE(state->AreEnabledInstanceExtensionsKnown());
  EXPECT_TRUE(
      state->HasEnabledInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME));
  EXPECT_TRUE(state->AreEnabledDeviceExtensionsKnown());
  EXPECT_EQ(state->HasEnabledDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
            !enabled_device_extensions.empty());

  device_fns.vkDestroyDevice(device, nullptr);
  functions.instance.vkDestroyInstance(instance, nullptr);
}

TEST(VulkanShaderFunctionVKTest, CreateShaderFunctionFromWGXModule) {
  auto context = skity::CreateGPUContextVK(vkGetInstanceProcAddr);
  ASSERT_NE(context, nullptr);

  auto* vk_context = static_cast<skity::GPUContextVK*>(context.get());
  ASSERT_NE(vk_context, nullptr);

  auto* context_impl = static_cast<skity::GPUContextImpl*>(vk_context);
  ASSERT_NE(context_impl, nullptr);
  auto* device = context_impl->GetGPUDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUShaderModuleDescriptor module_desc = {};
  module_desc.label = skity::GPULabel("vk_shader_module");
  module_desc.source = kSimpleVertexWGSL;
  auto module = skity::GPUShaderModule::Create(module_desc);
  ASSERT_NE(module, nullptr);

  skity::GPUShaderSourceWGX source = {};
  source.module = module;
  source.entry_point = "vs_main";

  skity::GPUShaderFunctionDescriptor function_desc = {};
  function_desc.label = skity::GPULabel("vk_shader_function");
  function_desc.stage = skity::GPUShaderStage::kVertex;
  function_desc.source_type = skity::GPUShaderSourceType::kWGX;
  function_desc.shader_source = &source;

  auto function = device->CreateShaderFunction(function_desc);
  ASSERT_NE(function, nullptr);
  EXPECT_TRUE(function->IsValid());
  EXPECT_EQ(function->GetLabel(), "vk_shader_function");

  auto vk_function =
      std::static_pointer_cast<skity::GPUShaderFunctionVK>(function);
  ASSERT_NE(vk_function, nullptr);
  EXPECT_EQ(vk_function->GetStage(), skity::GPUShaderStage::kVertex);
  EXPECT_EQ(vk_function->GetEntryPoint(), "vs_main");
  EXPECT_NE(vk_function->GetShaderModule(), VK_NULL_HANDLE);
}

TEST(VulkanShaderFunctionVKTest,
     FailToCreateShaderFunctionForMissingEntryPoint) {
  auto context = skity::CreateGPUContextVK(vkGetInstanceProcAddr);
  ASSERT_NE(context, nullptr);

  auto* vk_context = static_cast<skity::GPUContextVK*>(context.get());
  ASSERT_NE(vk_context, nullptr);

  auto* context_impl = static_cast<skity::GPUContextImpl*>(vk_context);
  ASSERT_NE(context_impl, nullptr);
  auto* device = context_impl->GetGPUDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUShaderModuleDescriptor module_desc = {};
  module_desc.label = skity::GPULabel("vk_shader_module");
  module_desc.source = kSimpleVertexWGSL;
  auto module = skity::GPUShaderModule::Create(module_desc);
  ASSERT_NE(module, nullptr);

  skity::GPUShaderSourceWGX source = {};
  source.module = module;
  source.entry_point = "missing_main";

  std::string error_message;
  skity::GPUShaderFunctionDescriptor function_desc = {};
  function_desc.label = skity::GPULabel("vk_shader_function");
  function_desc.stage = skity::GPUShaderStage::kVertex;
  function_desc.source_type = skity::GPUShaderSourceType::kWGX;
  function_desc.shader_source = &source;
  function_desc.error_callback = [&error_message](char const* message) {
    error_message = message != nullptr ? message : "";
  };

  auto function = device->CreateShaderFunction(function_desc);
  EXPECT_EQ(function, nullptr);
  EXPECT_EQ(error_message, "WGX translate to SPIR-V failed");
}

TEST(VulkanShaderFunctionVKTest, RejectRawShaderSource) {
  auto context = skity::CreateGPUContextVK(vkGetInstanceProcAddr);
  ASSERT_NE(context, nullptr);

  auto* vk_context = static_cast<skity::GPUContextVK*>(context.get());
  ASSERT_NE(vk_context, nullptr);

  auto* context_impl = static_cast<skity::GPUContextImpl*>(vk_context);
  ASSERT_NE(context_impl, nullptr);
  auto* device = context_impl->GetGPUDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUShaderSourceRaw source = {};
  source.source = "void main() {}";
  source.entry_point = "main";

  skity::GPUShaderFunctionDescriptor function_desc = {};
  function_desc.label = skity::GPULabel("vk_raw_shader");
  function_desc.stage = skity::GPUShaderStage::kVertex;
  function_desc.source_type = skity::GPUShaderSourceType::kRaw;
  function_desc.shader_source = &source;

  auto function = device->CreateShaderFunction(function_desc);
  EXPECT_EQ(function, nullptr);
}

}  // namespace
