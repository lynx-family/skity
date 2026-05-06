// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <skity/gpu/gpu_context_vk.hpp>
#include <string>
#include <vector>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_shader_module.hpp"
#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_context_impl_vk.hpp"
#include "src/gpu/vk/gpu_render_pipeline_vk.hpp"
#include "src/gpu/vk/gpu_sampler_vk.hpp"
#include "src/gpu/vk/gpu_shader_function_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/gpu/vk/vulkan_proc_table.hpp"

namespace {

constexpr char kSimpleVertexWGSL[] = R"(
@vertex
fn vs_main() -> @builtin(position) vec4<f32> {
  return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
)";

constexpr char kSimpleFragmentWGSL[] = R"(
@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}
)";

constexpr char kUniformFragmentWGSL[] = R"(
struct FSUniforms {
  color : vec4<f32>,
};

@group(0) @binding(0) var<uniform> fs_uniforms : FSUniforms;

@fragment
fn fs_uniform_main() -> @location(0) vec4<f32> {
  return fs_uniforms.color;
}
)";

constexpr char kUniformHelperVertexWGSL[] = R"(
struct CommonSlot {
  transform : mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> common_slot : CommonSlot;

fn get_vertex_position(a_pos: vec2<f32>, cs: CommonSlot) -> vec4<f32> {
  return cs.transform * vec4<f32>(a_pos, 0.0, 1.0);
}

@vertex
fn vs_main(@location(0) a_pos: vec2<f32>) -> @builtin(position) vec4<f32> {
  return get_vertex_position(a_pos, common_slot);
}
)";

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
constexpr bool kThreadSanitizerEnabled = true;
#else
constexpr bool kThreadSanitizerEnabled = false;
#endif
#else
constexpr bool kThreadSanitizerEnabled = false;
#endif

bool SkipVulkanTestsOnThreadSanitizer() { return kThreadSanitizerEnabled; }

bool SkipVulkanRenderPassTestsOnThreadSanitizer() {
  return kThreadSanitizerEnabled;
}

int32_t FindGraphicsQueueFamilyIndex(
    PFN_vkGetPhysicalDeviceQueueFamilyProperties
        vk_get_physical_device_queue_family_properties,
    VkPhysicalDevice physical_device);

bool ContainsLayer(const std::vector<VkLayerProperties>& layers,
                   const char* layer_name) {
  return std::any_of(layers.begin(), layers.end(),
                     [layer_name](const auto& layer) {
                       return std::string(layer.layerName) == layer_name;
                     });
}

bool ContainsLayer(const std::vector<std::string>& layers,
                   const char* layer_name) {
  return std::find(layers.begin(), layers.end(), layer_name) != layers.end();
}

std::unique_ptr<skity::GPUContext> CreateDebugGPUContext() {
  skity::GPUContextInfoVK info = {};
  info.get_instance_proc_addr = vkGetInstanceProcAddr;
  info.enable_debug_runtime = true;
  return skity::CreateGPUContextVK(&info);
}

struct LegacyContextBundle {
  std::unique_ptr<skity::GPUContext> context = nullptr;
  skity::VulkanFunctionPointers functions = {};
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;

  LegacyContextBundle() = default;
  LegacyContextBundle(const LegacyContextBundle&) = delete;
  LegacyContextBundle& operator=(const LegacyContextBundle&) = delete;
  LegacyContextBundle(LegacyContextBundle&& other) noexcept
      : context(std::move(other.context)),
        functions(other.functions),
        instance(other.instance),
        device(other.device) {
    other.instance = VK_NULL_HANDLE;
    other.device = VK_NULL_HANDLE;
    other.functions = {};
  }

  LegacyContextBundle& operator=(LegacyContextBundle&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    context.reset();
    if (device != VK_NULL_HANDLE) {
      functions.device.vkDeviceWaitIdle(device);
      functions.device.vkDestroyDevice(device, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
      functions.instance.vkDestroyInstance(instance, nullptr);
    }

    context = std::move(other.context);
    functions = other.functions;
    instance = other.instance;
    device = other.device;

    other.instance = VK_NULL_HANDLE;
    other.device = VK_NULL_HANDLE;
    other.functions = {};
    return *this;
  }

  ~LegacyContextBundle() {
    context.reset();
    if (device != VK_NULL_HANDLE) {
      functions.device.vkDeviceWaitIdle(device);
      functions.device.vkDestroyDevice(device, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
      functions.instance.vkDestroyInstance(instance, nullptr);
    }
  }
};

LegacyContextBundle CreateLegacyGPUContextForTest() {
  LegacyContextBundle bundle = {};

  if (!skity::CreateVkInstance(vkGetInstanceProcAddr, &bundle.instance,
                               &bundle.functions) ||
      bundle.instance == VK_NULL_HANDLE) {
    return std::move(bundle);
  }

  uint32_t physical_device_count = 0;
  if (bundle.functions.instance.vkEnumeratePhysicalDevices(
          bundle.instance, &physical_device_count, nullptr) != VK_SUCCESS ||
      physical_device_count == 0) {
    return std::move(bundle);
  }

  std::vector<VkPhysicalDevice> physical_devices(physical_device_count,
                                                 VK_NULL_HANDLE);
  if (bundle.functions.instance.vkEnumeratePhysicalDevices(
          bundle.instance, &physical_device_count, physical_devices.data()) !=
      VK_SUCCESS) {
    return std::move(bundle);
  }

  const VkPhysicalDevice physical_device = physical_devices[0];
  const int32_t graphics_queue_family_index = FindGraphicsQueueFamilyIndex(
      bundle.functions.instance.vkGetPhysicalDeviceQueueFamilyProperties,
      physical_device);
  if (graphics_queue_family_index < 0) {
    return std::move(bundle);
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

  if (bundle.functions.instance.vkCreateDevice(physical_device, &device_info,
                                               nullptr,
                                               &bundle.device) != VK_SUCCESS ||
      bundle.device == VK_NULL_HANDLE) {
    return std::move(bundle);
  }

  if (!skity::LoadVulkanDeviceFns(bundle.functions.instance.vkGetDeviceProcAddr,
                                  bundle.device, &bundle.functions.device)) {
    return std::move(bundle);
  }

  skity::GPUContextInfoVK info = {};
  info.instance = bundle.instance;
  info.get_instance_proc_addr = vkGetInstanceProcAddr;
  info.enabled_instance_extensions_known = true;
  info.physical_device = physical_device;
  info.logical_device = bundle.device;
  info.get_device_proc_addr = bundle.functions.instance.vkGetDeviceProcAddr;
  info.enabled_device_extension_count = 0;
  info.enabled_device_extensions_known = true;
  info.graphics_queue_family_index = graphics_queue_family_index;

  bundle.context = skity::CreateGPUContextVK(&info);
  return std::move(bundle);
}

class VulkanSharedContextTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { context_ = CreateDebugGPUContext(); }

  static void TearDownTestSuite() { context_.reset(); }

  static skity::GPUContext* GetContext() { return context_.get(); }

  static skity::GPUContextVK* GetVKContext() {
    return static_cast<skity::GPUContextVK*>(context_.get());
  }

  static skity::GPUContextImpl* GetContextImpl() {
    return static_cast<skity::GPUContextImpl*>(GetVKContext());
  }

  static skity::GPUDevice* GetDevice() {
    return GetContextImpl()->GetGPUDevice();
  }

  static const skity::VulkanContextState* GetState() {
    return GetVKContext()->GetState();
  }

  static std::unique_ptr<skity::GPUContext> context_;
};

std::unique_ptr<skity::GPUContext> VulkanSharedContextTest::context_ = nullptr;

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

std::shared_ptr<skity::GPUShaderFunction> CreateWGXShaderFunction(
    skity::GPUDevice* device, const char* source, const char* label,
    const char* entry_point, skity::GPUShaderStage stage) {
  skity::GPUShaderModuleDescriptor module_desc = {};
  module_desc.label = skity::GPULabel(label);
  module_desc.source = source;
  auto module = skity::GPUShaderModule::Create(module_desc);
  if (module == nullptr) {
    return nullptr;
  }

  skity::GPUShaderSourceWGX shader_source = {};
  shader_source.module = module;
  shader_source.entry_point = entry_point;

  skity::GPUShaderFunctionDescriptor function_desc = {};
  function_desc.label = skity::GPULabel(label);
  function_desc.stage = stage;
  function_desc.source_type = skity::GPUShaderSourceType::kWGX;
  function_desc.shader_source = &shader_source;
  return device->CreateShaderFunction(function_desc);
}

TEST(VulkanProcLoaderTest, CreateVkInstanceWithProcLoader) {
  if (SkipVulkanTestsOnThreadSanitizer()) {
    GTEST_SKIP() << "Vulkan tests are unstable under ThreadSanitizer with "
                    "SwiftShader in this environment";
  }
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
  if (SkipVulkanTestsOnThreadSanitizer()) {
    GTEST_SKIP() << "Vulkan tests are unstable under ThreadSanitizer with "
                    "SwiftShader in this environment";
  }
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

TEST_F(VulkanSharedContextTest, CreateGPUContextWithProcLoader) {
  ASSERT_NE(GetContext(), nullptr);
  EXPECT_EQ(GetContext()->GetBackendType(), skity::GPUBackendType::kVulkan);

  auto* vk_context = GetVKContext();
  ASSERT_NE(vk_context, nullptr);
  const auto* state = GetState();
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
  if (SkipVulkanTestsOnThreadSanitizer()) {
    GTEST_SKIP() << "Vulkan tests are unstable under ThreadSanitizer with "
                    "SwiftShader in this environment";
  }
  skity::GPUContextInfoVK info = {};
  info.get_instance_proc_addr = vkGetInstanceProcAddr;
  info.enable_debug_runtime = true;

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

TEST(VulkanProcLoaderTest, CreateGPUContextWithRequestedInstanceExtensions) {
  if (SkipVulkanTestsOnThreadSanitizer()) {
    GTEST_SKIP() << "Vulkan tests are unstable under ThreadSanitizer with "
                    "SwiftShader in this environment";
  }

  skity::VulkanGlobalFns global_fns = {};
  ASSERT_TRUE(skity::LoadVulkanGlobalFns(vkGetInstanceProcAddr, &global_fns));

  uint32_t extension_count = 0;
  ASSERT_EQ(global_fns.vkEnumerateInstanceExtensionProperties(
                nullptr, &extension_count, nullptr),
            VK_SUCCESS);
  ASSERT_GT(extension_count, 0u);

  std::vector<VkExtensionProperties> extensions(extension_count);
  ASSERT_EQ(global_fns.vkEnumerateInstanceExtensionProperties(
                nullptr, &extension_count, extensions.data()),
            VK_SUCCESS);

  const char* requested_extension = nullptr;
  for (const auto& extension : extensions) {
    if (std::string(extension.extensionName) ==
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME) {
      requested_extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
      break;
    }
  }

  if (requested_extension == nullptr) {
    GTEST_SKIP() << "VK_EXT_debug_utils is unavailable in this environment";
  }

  skity::GPUContextInfoVK info = {};
  info.get_instance_proc_addr = vkGetInstanceProcAddr;
  info.enabled_instance_extensions = &requested_extension;
  info.enabled_instance_extension_count = 1;

  auto context = skity::CreateGPUContextVK(&info);

  ASSERT_NE(context, nullptr);
  auto* vk_context = static_cast<skity::GPUContextVK*>(context.get());
  ASSERT_NE(vk_context, nullptr);
  const auto* state = vk_context->GetState();
  ASSERT_NE(state, nullptr);
  EXPECT_TRUE(state->HasEnabledInstanceExtension(requested_extension));
}

TEST(VulkanProcLoaderTest, DeduplicateRequestedInstanceExtensions) {
  if (SkipVulkanTestsOnThreadSanitizer()) {
    GTEST_SKIP() << "Vulkan tests are unstable under ThreadSanitizer with "
                    "SwiftShader in this environment";
  }

  const char* requested_extensions[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_SURFACE_EXTENSION_NAME,
  };

  skity::GPUContextInfoVK info = {};
  info.get_instance_proc_addr = vkGetInstanceProcAddr;
  info.enabled_instance_extensions = requested_extensions;
  info.enabled_instance_extension_count = 2;

  auto context = skity::CreateGPUContextVK(&info);

  ASSERT_NE(context, nullptr);
  auto* vk_context = static_cast<skity::GPUContextVK*>(context.get());
  ASSERT_NE(vk_context, nullptr);
  const auto* state = vk_context->GetState();
  ASSERT_NE(state, nullptr);

  size_t surface_extension_count = 0;
  for (const auto& extension : state->GetEnabledInstanceExtensions()) {
    if (extension == VK_KHR_SURFACE_EXTENSION_NAME) {
      ++surface_extension_count;
    }
  }
  EXPECT_EQ(surface_extension_count, 1u);
}

TEST(VulkanProcLoaderTest, PreserveUserProvidedExtensionInfo) {
  if (SkipVulkanTestsOnThreadSanitizer()) {
    GTEST_SKIP() << "Vulkan tests are unstable under ThreadSanitizer with "
                    "SwiftShader in this environment";
  }
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
        }
        if (std::string(extension.extensionName) ==
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) {
          enabled_device_extensions.push_back(
              VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
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
  info.enable_debug_runtime = true;

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
  EXPECT_EQ(state->IsSynchronization2Enabled(),
            std::find(enabled_device_extensions.begin(),
                      enabled_device_extensions.end(),
                      VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) !=
                enabled_device_extensions.end());

  device_fns.vkDestroyDevice(device, nullptr);
  functions.instance.vkDestroyInstance(instance, nullptr);
}

TEST_F(VulkanSharedContextTest, CreateShaderFunctionFromWGXModule) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
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

TEST_F(VulkanSharedContextTest,
       FailToCreateShaderFunctionForMissingEntryPoint) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
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

TEST_F(VulkanSharedContextTest, RejectRawShaderSource) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
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

TEST_F(VulkanSharedContextTest, CreateRenderPipelineFromWGXFunctions) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUShaderModuleDescriptor vertex_module_desc = {};
  vertex_module_desc.label = skity::GPULabel("vk_vertex_shader_module");
  vertex_module_desc.source = kSimpleVertexWGSL;
  auto vertex_module = skity::GPUShaderModule::Create(vertex_module_desc);
  ASSERT_NE(vertex_module, nullptr);

  skity::GPUShaderSourceWGX vertex_source = {};
  vertex_source.module = vertex_module;
  vertex_source.entry_point = "vs_main";

  skity::GPUShaderFunctionDescriptor vertex_desc = {};
  vertex_desc.label = skity::GPULabel("vk_vertex_shader_function");
  vertex_desc.stage = skity::GPUShaderStage::kVertex;
  vertex_desc.source_type = skity::GPUShaderSourceType::kWGX;
  vertex_desc.shader_source = &vertex_source;

  auto vertex_function = device->CreateShaderFunction(vertex_desc);
  ASSERT_NE(vertex_function, nullptr);
  ASSERT_TRUE(vertex_function->IsValid());

  skity::GPUShaderModuleDescriptor fragment_module_desc = {};
  fragment_module_desc.label = skity::GPULabel("vk_fragment_shader_module");
  fragment_module_desc.source = kSimpleFragmentWGSL;
  auto fragment_module = skity::GPUShaderModule::Create(fragment_module_desc);
  ASSERT_NE(fragment_module, nullptr);

  skity::GPUShaderSourceWGX fragment_source = {};
  fragment_source.module = fragment_module;
  fragment_source.entry_point = "fs_main";

  skity::GPUShaderFunctionDescriptor fragment_desc = {};
  fragment_desc.label = skity::GPULabel("vk_fragment_shader_function");
  fragment_desc.stage = skity::GPUShaderStage::kFragment;
  fragment_desc.source_type = skity::GPUShaderSourceType::kWGX;
  fragment_desc.shader_source = &fragment_source;

  auto fragment_function = device->CreateShaderFunction(fragment_desc);
  ASSERT_NE(fragment_function, nullptr);
  ASSERT_TRUE(fragment_function->IsValid());

  skity::GPURenderPipelineDescriptor pipeline_desc = {};
  pipeline_desc.vertex_function = vertex_function;
  pipeline_desc.fragment_function = fragment_function;
  pipeline_desc.target.format = skity::GPUTextureFormat::kRGBA8Unorm;
  pipeline_desc.sample_count = 1;
  pipeline_desc.label = skity::GPULabel("vk_render_pipeline");

  auto pipeline = device->CreateRenderPipeline(pipeline_desc);
  ASSERT_NE(pipeline, nullptr);
  ASSERT_TRUE(pipeline->IsValid());

  auto* vk_pipeline = skity::GPURenderPipelineVK::Cast(pipeline.get());
  ASSERT_NE(vk_pipeline, nullptr);
  EXPECT_NE(vk_pipeline->GetPipeline(), VK_NULL_HANDLE);
  EXPECT_NE(vk_pipeline->GetPipelineLayout(), VK_NULL_HANDLE);
  if (GetState()->IsDynamicRenderingEnabled()) {
    EXPECT_TRUE(vk_pipeline->UsesDynamicRendering());
    EXPECT_EQ(vk_pipeline->GetRenderPass(), VK_NULL_HANDLE);
  } else {
    EXPECT_FALSE(vk_pipeline->UsesDynamicRendering());
    EXPECT_NE(vk_pipeline->GetRenderPass(), VK_NULL_HANDLE);
  }
}

TEST_F(VulkanSharedContextTest,
       CreateRenderPipelineWithDepthStencilFormatFallback) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);

  auto vertex_function =
      CreateWGXShaderFunction(device, kSimpleVertexWGSL, "vk_depth_pipeline_vs",
                              "vs_main", skity::GPUShaderStage::kVertex);
  ASSERT_NE(vertex_function, nullptr);
  ASSERT_TRUE(vertex_function->IsValid());

  auto fragment_function = CreateWGXShaderFunction(
      device, kSimpleFragmentWGSL, "vk_depth_pipeline_fs", "fs_main",
      skity::GPUShaderStage::kFragment);
  ASSERT_NE(fragment_function, nullptr);
  ASSERT_TRUE(fragment_function->IsValid());

  skity::GPURenderPipelineDescriptor pipeline_desc = {};
  pipeline_desc.vertex_function = vertex_function;
  pipeline_desc.fragment_function = fragment_function;
  pipeline_desc.target.format = skity::GPUTextureFormat::kRGBA8Unorm;
  pipeline_desc.sample_count = 1;
  pipeline_desc.depth_stencil.format =
      skity::GPUTextureFormat::kDepth24Stencil8;
  pipeline_desc.depth_stencil.enable_depth = true;
  pipeline_desc.depth_stencil.depth_state.enableWrite = true;
  pipeline_desc.depth_stencil.depth_state.compare =
      skity::GPUCompareFunction::kLessEqual;
  pipeline_desc.label = skity::GPULabel("vk_depth_fallback_render_pipeline");

  auto pipeline = device->CreateRenderPipeline(pipeline_desc);
  ASSERT_NE(pipeline, nullptr);
  ASSERT_TRUE(pipeline->IsValid());
}

TEST_F(VulkanSharedContextTest,
       CreateRenderPipelineFromWGXFunctionsWithUniformHelperVertexShader) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);

  auto vertex_function = CreateWGXShaderFunction(
      device, kUniformHelperVertexWGSL, "vk_vertex_helper_uniform_shader",
      "vs_main", skity::GPUShaderStage::kVertex);
  ASSERT_NE(vertex_function, nullptr);
  ASSERT_TRUE(vertex_function->IsValid());

  auto fragment_function = CreateWGXShaderFunction(
      device, kSimpleFragmentWGSL, "vk_fragment_simple_shader", "fs_main",
      skity::GPUShaderStage::kFragment);
  ASSERT_NE(fragment_function, nullptr);
  ASSERT_TRUE(fragment_function->IsValid());

  skity::GPURenderPipelineDescriptor pipeline_desc = {};
  pipeline_desc.vertex_function = vertex_function;
  pipeline_desc.fragment_function = fragment_function;
  pipeline_desc.target.format = skity::GPUTextureFormat::kRGBA8Unorm;
  pipeline_desc.sample_count = 1;
  pipeline_desc.label = skity::GPULabel("vk_helper_uniform_render_pipeline");

  auto pipeline = device->CreateRenderPipeline(pipeline_desc);
  ASSERT_NE(pipeline, nullptr);
  ASSERT_TRUE(pipeline->IsValid());
}

TEST_F(VulkanSharedContextTest, CreateAndReuseSampler) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUSamplerDescriptor desc = {};
  desc.address_mode_u = skity::GPUAddressMode::kRepeat;
  desc.address_mode_v = skity::GPUAddressMode::kClampToEdge;
  desc.address_mode_w = skity::GPUAddressMode::kMirrorRepeat;
  desc.mag_filter = skity::GPUFilterMode::kLinear;
  desc.min_filter = skity::GPUFilterMode::kLinear;
  desc.mipmap_filter = skity::GPUMipmapMode::kLinear;

  auto sampler = device->CreateSampler(desc);
  ASSERT_NE(sampler, nullptr);

  auto* vk_sampler = skity::GPUSamplerVK::Cast(sampler.get());
  ASSERT_NE(vk_sampler, nullptr);
  EXPECT_TRUE(vk_sampler->IsValid());
  EXPECT_NE(vk_sampler->GetSampler(), VK_NULL_HANDLE);

  auto same_sampler = device->CreateSampler(desc);
  ASSERT_NE(same_sampler, nullptr);
  EXPECT_EQ(same_sampler.get(), sampler.get());
}

TEST_F(VulkanSharedContextTest, EncodeDrawCommandWithUniformBinding) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);

  auto vertex_function =
      CreateWGXShaderFunction(device, kSimpleVertexWGSL, "vk_draw_uniform_vs",
                              "vs_main", skity::GPUShaderStage::kVertex);
  ASSERT_NE(vertex_function, nullptr);
  ASSERT_TRUE(vertex_function->IsValid());

  auto fragment_function = CreateWGXShaderFunction(
      device, kUniformFragmentWGSL, "vk_draw_uniform_fs", "fs_uniform_main",
      skity::GPUShaderStage::kFragment);
  ASSERT_NE(fragment_function, nullptr);
  ASSERT_TRUE(fragment_function->IsValid());

  skity::GPURenderPipelineDescriptor pipeline_desc = {};
  pipeline_desc.vertex_function = vertex_function;
  pipeline_desc.fragment_function = fragment_function;
  pipeline_desc.target.format = skity::GPUTextureFormat::kRGBA8Unorm;
  pipeline_desc.sample_count = 1;
  pipeline_desc.label = skity::GPULabel("vk_draw_uniform_pipeline");

  auto pipeline = device->CreateRenderPipeline(pipeline_desc);
  ASSERT_NE(pipeline, nullptr);
  ASSERT_TRUE(pipeline->IsValid());

  auto vertex_buffer =
      device->CreateBuffer(skity::GPUBufferUsage::kVertexBuffer);
  auto index_buffer = device->CreateBuffer(skity::GPUBufferUsage::kIndexBuffer);
  auto uniform_buffer =
      device->CreateBuffer(skity::GPUBufferUsage::kUniformBuffer);
  ASSERT_NE(vertex_buffer, nullptr);
  ASSERT_NE(index_buffer, nullptr);
  ASSERT_NE(uniform_buffer, nullptr);

  skity::GPUTextureDescriptor texture_desc = {};
  texture_desc.width = 16;
  texture_desc.height = 16;
  texture_desc.mip_level_count = 1;
  texture_desc.sample_count = 1;
  texture_desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  texture_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  texture_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto texture = device->CreateTexture(texture_desc);
  ASSERT_NE(texture, nullptr);

  auto command_buffer = device->CreateCommandBuffer();
  ASSERT_NE(command_buffer, nullptr);

  float vertex_data[4] = {0.f, 0.f, 0.f, 1.f};
  uint32_t index_data[3] = {0u, 0u, 0u};
  float uniform_data[4] = {0.25f, 0.5f, 0.75f, 1.f};

  auto blit_pass = command_buffer->BeginBlitPass();
  ASSERT_NE(blit_pass, nullptr);
  blit_pass->UploadBufferData(vertex_buffer.get(), vertex_data,
                              sizeof(vertex_data));
  blit_pass->UploadBufferData(index_buffer.get(), index_data,
                              sizeof(index_data));
  blit_pass->UploadBufferData(uniform_buffer.get(), uniform_data,
                              sizeof(uniform_data));
  blit_pass->End();

  skity::GPURenderPassDescriptor render_pass_desc = {};
  render_pass_desc.label = "vk_draw_uniform_render_pass";
  render_pass_desc.color_attachment.texture = texture;
  render_pass_desc.color_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.color_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.color_attachment.clear_value =
      skity::GPUColor{0.0, 0.0, 0.0, 0.0};

  auto render_pass = command_buffer->BeginRenderPass(render_pass_desc);
  ASSERT_NE(render_pass, nullptr);

  skity::Command command = {};
  command.pipeline = pipeline.get();
  command.vertex_buffer = {vertex_buffer.get(), 0,
                           static_cast<uint32_t>(sizeof(vertex_data))};
  command.index_buffer = {index_buffer.get(), 0,
                          static_cast<uint32_t>(sizeof(index_data))};
  command.index_count = 3;
  command.scissor_rect = {0, 0, texture_desc.width, texture_desc.height};

  skity::UniformBinding uniform_binding = {};
  uniform_binding.stages =
      static_cast<skity::GPUShaderStageMask>(skity::GPUShaderStage::kFragment);
  uniform_binding.index = 0;
  uniform_binding.group = 0;
  uniform_binding.binding = 0;
  uniform_binding.name = "fs_uniforms";
  uniform_binding.buffer = {uniform_buffer.get(), 0,
                            static_cast<uint32_t>(sizeof(uniform_data))};
  command.uniform_bindings.push_back(uniform_binding);

  render_pass->AddCommand(&command);
  render_pass->EncodeCommands();

  EXPECT_TRUE(command_buffer->Submit());
  GetState()->CollectPendingSubmissions(true);
}

TEST(VulkanProcLoaderTest, EncodeLegacyDrawCommandWithUniformBinding) {
  auto bundle = CreateLegacyGPUContextForTest();
  ASSERT_NE(bundle.context, nullptr);

  auto* vk_context = static_cast<skity::GPUContextVK*>(bundle.context.get());
  ASSERT_NE(vk_context, nullptr);
  const auto* state = vk_context->GetState();
  ASSERT_NE(state, nullptr);
  EXPECT_FALSE(state->IsDynamicRenderingEnabled());

  auto* context_impl = static_cast<skity::GPUContextImpl*>(vk_context);
  auto* device = context_impl->GetGPUDevice();
  ASSERT_NE(device, nullptr);

  auto vertex_function = CreateWGXShaderFunction(
      device, kSimpleVertexWGSL, "legacy_vk_draw_uniform_vs", "vs_main",
      skity::GPUShaderStage::kVertex);
  ASSERT_NE(vertex_function, nullptr);
  ASSERT_TRUE(vertex_function->IsValid());

  auto fragment_function = CreateWGXShaderFunction(
      device, kUniformFragmentWGSL, "legacy_vk_draw_uniform_fs",
      "fs_uniform_main", skity::GPUShaderStage::kFragment);
  ASSERT_NE(fragment_function, nullptr);
  ASSERT_TRUE(fragment_function->IsValid());

  skity::GPURenderPipelineDescriptor pipeline_desc = {};
  pipeline_desc.vertex_function = vertex_function;
  pipeline_desc.fragment_function = fragment_function;
  pipeline_desc.target.format = skity::GPUTextureFormat::kRGBA8Unorm;
  pipeline_desc.sample_count = 1;
  pipeline_desc.label = skity::GPULabel("legacy_vk_draw_uniform_pipeline");

  auto pipeline = device->CreateRenderPipeline(pipeline_desc);
  ASSERT_NE(pipeline, nullptr);
  ASSERT_TRUE(pipeline->IsValid());

  auto vertex_buffer =
      device->CreateBuffer(skity::GPUBufferUsage::kVertexBuffer);
  auto index_buffer = device->CreateBuffer(skity::GPUBufferUsage::kIndexBuffer);
  auto uniform_buffer =
      device->CreateBuffer(skity::GPUBufferUsage::kUniformBuffer);
  ASSERT_NE(vertex_buffer, nullptr);
  ASSERT_NE(index_buffer, nullptr);
  ASSERT_NE(uniform_buffer, nullptr);

  skity::GPUTextureDescriptor texture_desc = {};
  texture_desc.width = 16;
  texture_desc.height = 16;
  texture_desc.mip_level_count = 1;
  texture_desc.sample_count = 1;
  texture_desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  texture_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  texture_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto texture = device->CreateTexture(texture_desc);
  ASSERT_NE(texture, nullptr);

  auto command_buffer = device->CreateCommandBuffer();
  ASSERT_NE(command_buffer, nullptr);

  float vertex_data[4] = {0.f, 0.f, 0.f, 1.f};
  uint32_t index_data[3] = {0u, 0u, 0u};
  float uniform_data[4] = {1.f, 0.f, 0.f, 1.f};

  auto blit_pass = command_buffer->BeginBlitPass();
  ASSERT_NE(blit_pass, nullptr);
  blit_pass->UploadBufferData(vertex_buffer.get(), vertex_data,
                              sizeof(vertex_data));
  blit_pass->UploadBufferData(index_buffer.get(), index_data,
                              sizeof(index_data));
  blit_pass->UploadBufferData(uniform_buffer.get(), uniform_data,
                              sizeof(uniform_data));
  blit_pass->End();

  skity::GPURenderPassDescriptor render_pass_desc = {};
  render_pass_desc.label = "legacy_vk_draw_uniform_render_pass";
  render_pass_desc.color_attachment.texture = texture;
  render_pass_desc.color_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.color_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.color_attachment.clear_value =
      skity::GPUColor{0.0, 0.0, 0.0, 0.0};

  auto render_pass = command_buffer->BeginRenderPass(render_pass_desc);
  ASSERT_NE(render_pass, nullptr);

  skity::Command command = {};
  command.pipeline = pipeline.get();
  command.vertex_buffer = {vertex_buffer.get(), 0,
                           static_cast<uint32_t>(sizeof(vertex_data))};
  command.index_buffer = {index_buffer.get(), 0,
                          static_cast<uint32_t>(sizeof(index_data))};
  command.index_count = 3;
  command.scissor_rect = {0, 0, texture_desc.width, texture_desc.height};

  skity::UniformBinding uniform_binding = {};
  uniform_binding.stages =
      static_cast<skity::GPUShaderStageMask>(skity::GPUShaderStage::kFragment);
  uniform_binding.index = 0;
  uniform_binding.group = 0;
  uniform_binding.binding = 0;
  uniform_binding.name = "fs_uniforms";
  uniform_binding.buffer = {uniform_buffer.get(), 0,
                            static_cast<uint32_t>(sizeof(uniform_data))};
  command.uniform_bindings.push_back(uniform_binding);

  render_pass->AddCommand(&command);
  render_pass->EncodeCommands();

  EXPECT_TRUE(command_buffer->Submit());
  state->CollectPendingSubmissions(true);
}

TEST(VulkanProcLoaderTest, CreateLegacyRenderPipelineFromWGXFunctions) {
  auto bundle = CreateLegacyGPUContextForTest();
  ASSERT_NE(bundle.context, nullptr);

  auto* vk_context = static_cast<skity::GPUContextVK*>(bundle.context.get());
  ASSERT_NE(vk_context, nullptr);
  const auto* state = vk_context->GetState();
  ASSERT_NE(state, nullptr);
  EXPECT_FALSE(state->IsDynamicRenderingEnabled());

  auto* context_impl = static_cast<skity::GPUContextImpl*>(vk_context);
  auto* device = context_impl->GetGPUDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUShaderModuleDescriptor vertex_module_desc = {};
  vertex_module_desc.label = skity::GPULabel("legacy_vk_vertex_shader_module");
  vertex_module_desc.source = kSimpleVertexWGSL;
  auto vertex_module = skity::GPUShaderModule::Create(vertex_module_desc);
  ASSERT_NE(vertex_module, nullptr);

  skity::GPUShaderSourceWGX vertex_source = {};
  vertex_source.module = vertex_module;
  vertex_source.entry_point = "vs_main";

  skity::GPUShaderFunctionDescriptor vertex_desc = {};
  vertex_desc.label = skity::GPULabel("legacy_vk_vertex_shader_function");
  vertex_desc.stage = skity::GPUShaderStage::kVertex;
  vertex_desc.source_type = skity::GPUShaderSourceType::kWGX;
  vertex_desc.shader_source = &vertex_source;

  auto vertex_function = device->CreateShaderFunction(vertex_desc);
  ASSERT_NE(vertex_function, nullptr);
  ASSERT_TRUE(vertex_function->IsValid());

  skity::GPUShaderModuleDescriptor fragment_module_desc = {};
  fragment_module_desc.label =
      skity::GPULabel("legacy_vk_fragment_shader_module");
  fragment_module_desc.source = kSimpleFragmentWGSL;
  auto fragment_module = skity::GPUShaderModule::Create(fragment_module_desc);
  ASSERT_NE(fragment_module, nullptr);

  skity::GPUShaderSourceWGX fragment_source = {};
  fragment_source.module = fragment_module;
  fragment_source.entry_point = "fs_main";

  skity::GPUShaderFunctionDescriptor fragment_desc = {};
  fragment_desc.label = skity::GPULabel("legacy_vk_fragment_shader_function");
  fragment_desc.stage = skity::GPUShaderStage::kFragment;
  fragment_desc.source_type = skity::GPUShaderSourceType::kWGX;
  fragment_desc.shader_source = &fragment_source;

  auto fragment_function = device->CreateShaderFunction(fragment_desc);
  ASSERT_NE(fragment_function, nullptr);
  ASSERT_TRUE(fragment_function->IsValid());

  skity::GPURenderPipelineDescriptor pipeline_desc = {};
  pipeline_desc.vertex_function = vertex_function;
  pipeline_desc.fragment_function = fragment_function;
  pipeline_desc.target.format = skity::GPUTextureFormat::kRGBA8Unorm;
  pipeline_desc.sample_count = 1;
  pipeline_desc.label = skity::GPULabel("legacy_vk_render_pipeline");

  auto pipeline = device->CreateRenderPipeline(pipeline_desc);
  ASSERT_NE(pipeline, nullptr);
  ASSERT_TRUE(pipeline->IsValid());

  auto* vk_pipeline = skity::GPURenderPipelineVK::Cast(pipeline.get());
  ASSERT_NE(vk_pipeline, nullptr);
  EXPECT_FALSE(vk_pipeline->UsesDynamicRendering());
  EXPECT_NE(vk_pipeline->GetPipeline(), VK_NULL_HANDLE);
  EXPECT_NE(vk_pipeline->GetPipelineLayout(), VK_NULL_HANDLE);
  EXPECT_NE(vk_pipeline->GetRenderPass(), VK_NULL_HANDLE);
}

TEST_F(VulkanSharedContextTest, CreateAndUploadBufferData) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);

  auto buffer = device->CreateBuffer(skity::GPUBufferUsage::kVertexBuffer |
                                     skity::GPUBufferUsage::kUniformBuffer);
  ASSERT_NE(buffer, nullptr);

  auto* vk_buffer = static_cast<skity::GPUBufferVK*>(buffer.get());
  ASSERT_NE(vk_buffer, nullptr);
  EXPECT_FALSE(vk_buffer->IsValid());
  EXPECT_EQ(vk_buffer->GetMemoryType(),
            skity::GPUBufferVKMemoryType::kDeviceLocal);

  const uint32_t payload[] = {1u, 2u, 3u, 4u};
  auto command_buffer = device->CreateCommandBuffer();
  ASSERT_NE(command_buffer, nullptr);

  auto blit_pass = command_buffer->BeginBlitPass();
  ASSERT_NE(blit_pass, nullptr);
  blit_pass->UploadBufferData(vk_buffer, const_cast<uint32_t*>(payload),
                              sizeof(payload));
  blit_pass->End();
  ASSERT_TRUE(command_buffer->Submit());
  GetState()->CollectPendingSubmissions(true);

  EXPECT_TRUE(vk_buffer->IsValid());
  EXPECT_NE(vk_buffer->GetBuffer(), VK_NULL_HANDLE);
  EXPECT_NE(vk_buffer->GetAllocation(), VK_NULL_HANDLE);
  EXPECT_EQ(vk_buffer->GetMappedData(), nullptr);
  EXPECT_GE(vk_buffer->GetSize(), sizeof(payload));
}

TEST_F(VulkanSharedContextTest, CreateTextureAndUploadData) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUTextureDescriptor desc = {};
  desc.width = 4;
  desc.height = 4;
  desc.mip_level_count = 1;
  desc.sample_count = 1;
  desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  desc.usage =
      static_cast<skity::GPUTextureUsageMask>(
          skity::GPUTextureUsage::kTextureBinding) |
      static_cast<skity::GPUTextureUsageMask>(skity::GPUTextureUsage::kCopyDst);
  desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto texture = device->CreateTexture(desc);
  ASSERT_NE(texture, nullptr);

  auto* vk_texture = skity::GPUTextureVK::Cast(texture.get());
  ASSERT_NE(vk_texture, nullptr);
  EXPECT_TRUE(vk_texture->IsValid());
  EXPECT_NE(vk_texture->GetImage(), VK_NULL_HANDLE);
  EXPECT_NE(vk_texture->GetImageView(), VK_NULL_HANDLE);
  EXPECT_EQ(vk_texture->GetDescriptor().width, desc.width);
  EXPECT_EQ(vk_texture->GetDescriptor().height, desc.height);
  EXPECT_EQ(vk_texture->GetDescriptor().format, desc.format);
  EXPECT_EQ(vk_texture->GetPreferredLayout(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  EXPECT_EQ(vk_texture->GetCurrentLayout(), VK_IMAGE_LAYOUT_UNDEFINED);
  EXPECT_EQ(vk_texture->GetBytes(), 4u * 4u * 4u);

  uint32_t pixels[16] = {};
  for (size_t i = 0; i < std::size(pixels); ++i) {
    pixels[i] = 0xFF000000u | static_cast<uint32_t>(i);
  }

  texture->UploadData(0, 0, 4, 4, pixels);
  GetState()->CollectPendingSubmissions(true);

  EXPECT_EQ(vk_texture->GetCurrentLayout(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST_F(VulkanSharedContextTest, CollectPendingSubmissionsRunsCleanupActions) {
  ASSERT_NE(GetState(), nullptr);

  bool cleanup_ran = false;
  std::vector<std::function<void()>> cleanup_actions;
  cleanup_actions.emplace_back([&cleanup_ran]() { cleanup_ran = true; });

  GetState()->EnqueuePendingSubmission(skity::VulkanPendingSubmission(
      VK_NULL_HANDLE, VK_NULL_HANDLE, {}, std::move(cleanup_actions)));
  GetState()->CollectPendingSubmissions(true);

  EXPECT_TRUE(cleanup_ran);
}

TEST_F(VulkanSharedContextTest, CreateTextureBackedSurfaceAndFlush) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUTextureDescriptor texture_desc = {};
  texture_desc.width = 16;
  texture_desc.height = 16;
  texture_desc.mip_level_count = 1;
  texture_desc.sample_count = 1;
  texture_desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  texture_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  texture_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto texture = device->CreateTexture(texture_desc);
  ASSERT_NE(texture, nullptr);

  auto* vk_texture = skity::GPUTextureVK::Cast(texture.get());
  ASSERT_NE(vk_texture, nullptr);
  ASSERT_TRUE(vk_texture->IsValid());

  skity::GPUSurfaceDescriptorVK surface_desc = {};
  surface_desc.backend = skity::GPUBackendType::kVulkan;
  surface_desc.width = texture_desc.width;
  surface_desc.height = texture_desc.height;
  surface_desc.sample_count = 1;
  surface_desc.content_scale = 1.f;
  surface_desc.surface_type = skity::VKSurfaceType::kTexture;
  surface_desc.image = vk_texture->GetImage();
  surface_desc.image_view = vk_texture->GetImageView();
  surface_desc.format = vk_texture->GetVkFormat();
  surface_desc.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  surface_desc.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  auto surface = GetContext()->CreateSurface(&surface_desc);
  ASSERT_NE(surface, nullptr);

  auto* canvas = surface->LockCanvas();
  ASSERT_NE(canvas, nullptr);

  skity::Paint paint;
  paint.SetColor(skity::ColorSetARGB(255, 255, 0, 0));
  canvas->DrawRect(skity::Rect::MakeWH(16.f, 16.f), paint);
  canvas->Flush();
  surface->Flush();

  GetState()->CollectPendingSubmissions(true);
}

TEST_F(VulkanSharedContextTest, CreateMultiUseTextureUsesGeneralLayout) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUTextureDescriptor desc = {};
  desc.width = 8;
  desc.height = 8;
  desc.mip_level_count = 1;
  desc.sample_count = 1;
  desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  desc.usage = static_cast<skity::GPUTextureUsageMask>(
                   skity::GPUTextureUsage::kTextureBinding) |
               static_cast<skity::GPUTextureUsageMask>(
                   skity::GPUTextureUsage::kRenderAttachment);
  desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto texture = device->CreateTexture(desc);
  ASSERT_NE(texture, nullptr);

  auto* vk_texture = skity::GPUTextureVK::Cast(texture.get());
  ASSERT_NE(vk_texture, nullptr);
  EXPECT_TRUE(vk_texture->IsValid());
  EXPECT_EQ(vk_texture->GetPreferredLayout(), VK_IMAGE_LAYOUT_GENERAL);
  EXPECT_EQ(vk_texture->GetCurrentLayout(), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST_F(VulkanSharedContextTest, RejectTextureWithInvalidDescriptor) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUTextureDescriptor invalid_desc = {};
  invalid_desc.width = 0;
  invalid_desc.height = 8;
  invalid_desc.mip_level_count = 1;
  invalid_desc.sample_count = 1;
  invalid_desc.format = skity::GPUTextureFormat::kInvalid;
  invalid_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kTextureBinding);
  invalid_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  EXPECT_EQ(device->CreateTexture(invalid_desc), nullptr);
}

TEST_F(VulkanSharedContextTest, BeginAndEndRenderPassWithoutCommands) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);
  if (SkipVulkanRenderPassTestsOnThreadSanitizer()) {
    GTEST_SKIP() << "Vulkan render pass tests are unstable under "
                    "ThreadSanitizer with SwiftShader in this environment";
  }
  if (!GetState()->IsDynamicRenderingEnabled()) {
    GTEST_SKIP() << "Dynamic rendering is not enabled in this Vulkan context";
  }

  skity::GPUTextureDescriptor desc = {};
  desc.width = 16;
  desc.height = 16;
  desc.mip_level_count = 1;
  desc.sample_count = 1;
  desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  desc.usage = static_cast<skity::GPUTextureUsageMask>(
                   skity::GPUTextureUsage::kTextureBinding) |
               static_cast<skity::GPUTextureUsageMask>(
                   skity::GPUTextureUsage::kRenderAttachment);
  desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto texture = device->CreateTexture(desc);
  ASSERT_NE(texture, nullptr);

  skity::GPURenderPassDescriptor render_pass_desc = {};
  render_pass_desc.label = "vk_dynamic_render_pass";
  render_pass_desc.color_attachment.texture = texture;
  render_pass_desc.color_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.color_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.color_attachment.clear_value =
      skity::GPUColor{0.25, 0.5, 0.75, 1.0};

  auto command_buffer = device->CreateCommandBuffer();
  ASSERT_NE(command_buffer, nullptr);

  auto render_pass = command_buffer->BeginRenderPass(render_pass_desc);
  ASSERT_NE(render_pass, nullptr);
  render_pass->EncodeCommands();
  EXPECT_TRUE(command_buffer->Submit());
  GetState()->CollectPendingSubmissions(true);

  auto* vk_texture = skity::GPUTextureVK::Cast(texture.get());
  ASSERT_NE(vk_texture, nullptr);
  EXPECT_EQ(vk_texture->GetCurrentLayout(), VK_IMAGE_LAYOUT_GENERAL);
}

TEST_F(VulkanSharedContextTest, EnablesValidationLayerWhenAvailable) {
  ASSERT_NE(GetContext(), nullptr);
  const auto* state = GetState();
  ASSERT_NE(state, nullptr);
  ASSERT_TRUE(state->AreEnabledInstanceLayersKnown());

  constexpr char kValidationLayer[] = "VK_LAYER_KHRONOS_validation";
  if (!ContainsLayer(state->GetAvailableInstanceLayers(), kValidationLayer)) {
    GTEST_SKIP() << "Vulkan validation layer is not available";
  }

  EXPECT_TRUE(
      ContainsLayer(state->GetEnabledInstanceLayers(), kValidationLayer));
}

TEST_F(VulkanSharedContextTest, EnablesPortabilitySubsetWhenAvailable) {
  ASSERT_NE(GetContext(), nullptr);
  const auto* state = GetState();
  ASSERT_NE(state, nullptr);
  ASSERT_TRUE(state->AreEnabledDeviceExtensionsKnown());

  constexpr char kPortabilitySubsetExtension[] = "VK_KHR_portability_subset";
  if (!state->HasAvailableDeviceExtension(kPortabilitySubsetExtension)) {
    GTEST_SKIP() << "Vulkan portability subset extension is not available";
  }

  EXPECT_TRUE(state->HasEnabledDeviceExtension(kPortabilitySubsetExtension));
}

TEST_F(VulkanSharedContextTest, BeginAndEndMSAARenderPassWithoutCommands) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);
  if (SkipVulkanRenderPassTestsOnThreadSanitizer()) {
    GTEST_SKIP() << "Vulkan render pass tests are unstable under "
                    "ThreadSanitizer with SwiftShader in this environment";
  }
  if (!GetState()->IsDynamicRenderingEnabled()) {
    GTEST_SKIP() << "Dynamic rendering is not enabled in this Vulkan context";
  }

  skity::GPUTextureDescriptor msaa_desc = {};
  msaa_desc.width = 16;
  msaa_desc.height = 16;
  msaa_desc.mip_level_count = 1;
  msaa_desc.sample_count = 4;
  msaa_desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  msaa_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  msaa_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto msaa_texture = device->CreateTexture(msaa_desc);
  ASSERT_NE(msaa_texture, nullptr);

  skity::GPUTextureDescriptor resolve_desc = {};
  resolve_desc.width = msaa_desc.width;
  resolve_desc.height = msaa_desc.height;
  resolve_desc.mip_level_count = 1;
  resolve_desc.sample_count = 1;
  resolve_desc.format = msaa_desc.format;
  resolve_desc.usage = static_cast<skity::GPUTextureUsageMask>(
                           skity::GPUTextureUsage::kTextureBinding) |
                       static_cast<skity::GPUTextureUsageMask>(
                           skity::GPUTextureUsage::kRenderAttachment);
  resolve_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto resolve_texture = device->CreateTexture(resolve_desc);
  ASSERT_NE(resolve_texture, nullptr);

  skity::GPURenderPassDescriptor render_pass_desc = {};
  render_pass_desc.label = "vk_dynamic_msaa_render_pass";
  render_pass_desc.color_attachment.texture = msaa_texture;
  render_pass_desc.color_attachment.resolve_texture = resolve_texture;
  render_pass_desc.color_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.color_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.color_attachment.clear_value =
      skity::GPUColor{0.1, 0.2, 0.3, 1.0};

  auto command_buffer = device->CreateCommandBuffer();
  ASSERT_NE(command_buffer, nullptr);

  auto render_pass = command_buffer->BeginRenderPass(render_pass_desc);
  ASSERT_NE(render_pass, nullptr);
  render_pass->EncodeCommands();
  EXPECT_TRUE(command_buffer->Submit());
  GetState()->CollectPendingSubmissions(true);

  auto* vk_msaa_texture = skity::GPUTextureVK::Cast(msaa_texture.get());
  ASSERT_NE(vk_msaa_texture, nullptr);
  EXPECT_EQ(vk_msaa_texture->GetCurrentLayout(), VK_IMAGE_LAYOUT_GENERAL);

  auto* vk_resolve_texture = skity::GPUTextureVK::Cast(resolve_texture.get());
  ASSERT_NE(vk_resolve_texture, nullptr);
  EXPECT_EQ(vk_resolve_texture->GetCurrentLayout(), VK_IMAGE_LAYOUT_GENERAL);
}

TEST_F(VulkanSharedContextTest,
       BeginAndEndDepthStencilRenderPassWithoutCommands) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);
  if (SkipVulkanRenderPassTestsOnThreadSanitizer()) {
    GTEST_SKIP() << "Vulkan render pass tests are unstable under "
                    "ThreadSanitizer with SwiftShader in this environment";
  }
  if (!GetState()->IsDynamicRenderingEnabled()) {
    GTEST_SKIP() << "Dynamic rendering is not enabled in this Vulkan context";
  }

  skity::GPUTextureDescriptor color_desc = {};
  color_desc.width = 12;
  color_desc.height = 12;
  color_desc.mip_level_count = 1;
  color_desc.sample_count = 1;
  color_desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  color_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  color_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto color_texture = device->CreateTexture(color_desc);
  ASSERT_NE(color_texture, nullptr);

  skity::GPUTextureDescriptor depth_desc = {};
  depth_desc.width = color_desc.width;
  depth_desc.height = color_desc.height;
  depth_desc.mip_level_count = 1;
  depth_desc.sample_count = 1;
  depth_desc.format = skity::GPUTextureFormat::kDepth24Stencil8;
  depth_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  depth_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto depth_stencil_texture = device->CreateTexture(depth_desc);
  ASSERT_NE(depth_stencil_texture, nullptr);

  skity::GPURenderPassDescriptor render_pass_desc = {};
  render_pass_desc.label = "vk_dynamic_depth_stencil_render_pass";
  render_pass_desc.color_attachment.texture = color_texture;
  render_pass_desc.color_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.color_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.depth_attachment.texture = depth_stencil_texture;
  render_pass_desc.depth_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.depth_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.depth_attachment.clear_value = 0.5f;
  render_pass_desc.stencil_attachment.texture = depth_stencil_texture;
  render_pass_desc.stencil_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.stencil_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.stencil_attachment.clear_value = 7u;

  auto command_buffer = device->CreateCommandBuffer();
  ASSERT_NE(command_buffer, nullptr);

  auto render_pass = command_buffer->BeginRenderPass(render_pass_desc);
  ASSERT_NE(render_pass, nullptr);
  render_pass->EncodeCommands();
  EXPECT_TRUE(command_buffer->Submit());
  GetState()->CollectPendingSubmissions(true);

  auto* vk_texture = skity::GPUTextureVK::Cast(depth_stencil_texture.get());
  ASSERT_NE(vk_texture, nullptr);
  EXPECT_EQ(vk_texture->GetCurrentLayout(),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

TEST_F(VulkanSharedContextTest, BeginAndEndStencilRenderPassWithoutCommands) {
  ASSERT_NE(GetContext(), nullptr);
  auto* device = GetDevice();
  ASSERT_NE(device, nullptr);
  if (SkipVulkanRenderPassTestsOnThreadSanitizer()) {
    GTEST_SKIP() << "Vulkan render pass tests are unstable under "
                    "ThreadSanitizer with SwiftShader in this environment";
  }
  if (!GetState()->IsDynamicRenderingEnabled()) {
    GTEST_SKIP() << "Dynamic rendering is not enabled in this Vulkan context";
  }

  skity::GPUTextureDescriptor color_desc = {};
  color_desc.width = 14;
  color_desc.height = 14;
  color_desc.mip_level_count = 1;
  color_desc.sample_count = 1;
  color_desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  color_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  color_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto color_texture = device->CreateTexture(color_desc);
  ASSERT_NE(color_texture, nullptr);

  skity::GPUTextureDescriptor stencil_desc = {};
  stencil_desc.width = color_desc.width;
  stencil_desc.height = color_desc.height;
  stencil_desc.mip_level_count = 1;
  stencil_desc.sample_count = 1;
  stencil_desc.format = skity::GPUTextureFormat::kStencil8;
  stencil_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  stencil_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto stencil_texture = device->CreateTexture(stencil_desc);
  ASSERT_NE(stencil_texture, nullptr);

  skity::GPURenderPassDescriptor render_pass_desc = {};
  render_pass_desc.label = "vk_dynamic_stencil_render_pass";
  render_pass_desc.color_attachment.texture = color_texture;
  render_pass_desc.color_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.color_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.stencil_attachment.texture = stencil_texture;
  render_pass_desc.stencil_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.stencil_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.stencil_attachment.clear_value = 5u;

  auto command_buffer = device->CreateCommandBuffer();
  ASSERT_NE(command_buffer, nullptr);

  auto render_pass = command_buffer->BeginRenderPass(render_pass_desc);
  ASSERT_NE(render_pass, nullptr);
  render_pass->EncodeCommands();
  EXPECT_TRUE(command_buffer->Submit());
  GetState()->CollectPendingSubmissions(true);

  auto* vk_texture = skity::GPUTextureVK::Cast(stencil_texture.get());
  ASSERT_NE(vk_texture, nullptr);
  EXPECT_EQ(vk_texture->GetCurrentLayout(),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

TEST(VulkanProcLoaderTest, BeginAndEndLegacyRenderPassWithoutCommands) {
  auto bundle = CreateLegacyGPUContextForTest();
  ASSERT_NE(bundle.context, nullptr);

  auto* vk_context = static_cast<skity::GPUContextVK*>(bundle.context.get());
  ASSERT_NE(vk_context, nullptr);
  const auto* state = vk_context->GetState();
  ASSERT_NE(state, nullptr);
  EXPECT_FALSE(state->IsDynamicRenderingEnabled());

  auto* context_impl = static_cast<skity::GPUContextImpl*>(vk_context);
  auto* device = context_impl->GetGPUDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUTextureDescriptor desc = {};
  desc.width = 8;
  desc.height = 8;
  desc.mip_level_count = 1;
  desc.sample_count = 1;
  desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto texture = device->CreateTexture(desc);
  ASSERT_NE(texture, nullptr);

  skity::GPURenderPassDescriptor render_pass_desc = {};
  render_pass_desc.label = "vk_legacy_render_pass";
  render_pass_desc.color_attachment.texture = texture;
  render_pass_desc.color_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.color_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.color_attachment.clear_value =
      skity::GPUColor{0.0, 0.0, 0.0, 0.0};

  auto command_buffer = device->CreateCommandBuffer();
  ASSERT_NE(command_buffer, nullptr);

  auto render_pass = command_buffer->BeginRenderPass(render_pass_desc);
  ASSERT_NE(render_pass, nullptr);
  render_pass->EncodeCommands();
  EXPECT_TRUE(command_buffer->Submit());
  state->CollectPendingSubmissions(true);

  auto* vk_texture = skity::GPUTextureVK::Cast(texture.get());
  ASSERT_NE(vk_texture, nullptr);
  EXPECT_EQ(vk_texture->GetCurrentLayout(),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

TEST(VulkanProcLoaderTest,
     BeginAndEndLegacyDepthStencilRenderPassWithoutCommands) {
  auto bundle = CreateLegacyGPUContextForTest();
  ASSERT_NE(bundle.context, nullptr);

  auto* vk_context = static_cast<skity::GPUContextVK*>(bundle.context.get());
  ASSERT_NE(vk_context, nullptr);
  const auto* state = vk_context->GetState();
  ASSERT_NE(state, nullptr);
  EXPECT_FALSE(state->IsDynamicRenderingEnabled());

  auto* context_impl = static_cast<skity::GPUContextImpl*>(vk_context);
  auto* device = context_impl->GetGPUDevice();
  ASSERT_NE(device, nullptr);

  skity::GPUTextureDescriptor color_desc = {};
  color_desc.width = 10;
  color_desc.height = 10;
  color_desc.mip_level_count = 1;
  color_desc.sample_count = 1;
  color_desc.format = skity::GPUTextureFormat::kRGBA8Unorm;
  color_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  color_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto color_texture = device->CreateTexture(color_desc);
  ASSERT_NE(color_texture, nullptr);

  skity::GPUTextureDescriptor depth_desc = {};
  depth_desc.width = color_desc.width;
  depth_desc.height = color_desc.height;
  depth_desc.mip_level_count = 1;
  depth_desc.sample_count = 1;
  depth_desc.format = skity::GPUTextureFormat::kDepth24Stencil8;
  depth_desc.usage = static_cast<skity::GPUTextureUsageMask>(
      skity::GPUTextureUsage::kRenderAttachment);
  depth_desc.storage_mode = skity::GPUTextureStorageMode::kPrivate;

  auto depth_stencil_texture = device->CreateTexture(depth_desc);
  ASSERT_NE(depth_stencil_texture, nullptr);

  skity::GPURenderPassDescriptor render_pass_desc = {};
  render_pass_desc.label = "vk_legacy_depth_stencil_render_pass";
  render_pass_desc.color_attachment.texture = color_texture;
  render_pass_desc.color_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.color_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.depth_attachment.texture = depth_stencil_texture;
  render_pass_desc.depth_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.depth_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.depth_attachment.clear_value = 1.0f;
  render_pass_desc.stencil_attachment.texture = depth_stencil_texture;
  render_pass_desc.stencil_attachment.load_op = skity::GPULoadOp::kClear;
  render_pass_desc.stencil_attachment.store_op = skity::GPUStoreOp::kStore;
  render_pass_desc.stencil_attachment.clear_value = 3u;

  auto command_buffer = device->CreateCommandBuffer();
  ASSERT_NE(command_buffer, nullptr);

  auto render_pass = command_buffer->BeginRenderPass(render_pass_desc);
  ASSERT_NE(render_pass, nullptr);
  render_pass->EncodeCommands();
  EXPECT_TRUE(command_buffer->Submit());
  state->CollectPendingSubmissions(true);

  auto* vk_texture = skity::GPUTextureVK::Cast(depth_stencil_texture.get());
  ASSERT_NE(vk_texture, nullptr);
  EXPECT_EQ(vk_texture->GetCurrentLayout(),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

TEST(VulkanProcLoaderTest, LegacyRenderPassCacheReusesCompatibleRenderPass) {
  auto bundle = CreateLegacyGPUContextForTest();
  ASSERT_NE(bundle.context, nullptr);

  auto* vk_context = static_cast<skity::GPUContextVK*>(bundle.context.get());
  ASSERT_NE(vk_context, nullptr);
  const auto* state = vk_context->GetState();
  ASSERT_NE(state, nullptr);
  EXPECT_FALSE(state->IsDynamicRenderingEnabled());

  skity::VulkanContextState::LegacyRenderPassKey key = {};
  key.color_format = VK_FORMAT_R8G8B8A8_UNORM;
  key.color_samples = VK_SAMPLE_COUNT_1_BIT;
  key.color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
  key.color_store_op = VK_ATTACHMENT_STORE_OP_STORE;

  const VkRenderPass first = state->GetOrCreateLegacyRenderPass(key);
  ASSERT_NE(first, VK_NULL_HANDLE);

  const VkRenderPass second = state->GetOrCreateLegacyRenderPass(key);
  EXPECT_EQ(first, second);

  key.has_stencil = true;
  key.depth_stencil_format = VK_FORMAT_S8_UINT;
  key.depth_stencil_samples = VK_SAMPLE_COUNT_1_BIT;
  key.stencil_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
  key.stencil_store_op = VK_ATTACHMENT_STORE_OP_STORE;

  const VkRenderPass third = state->GetOrCreateLegacyRenderPass(key);
  ASSERT_NE(third, VK_NULL_HANDLE);
  EXPECT_NE(first, third);
}

TEST(GPUNativeWindowVKTest, CreateRejectsNullInfo) {
  auto context = skity::CreateGPUNativeWindowVK(
      static_cast<skity::GPUContext*>(nullptr), nullptr);
  EXPECT_EQ(context, nullptr);
}

TEST(GPUNativeWindowVKTest, CreateRejectsMissingContext) {
  skity::GPUNativeWindowInfoVK info = {};
  info.native_window.type = skity::VKNativeWindowType::kMetalLayer;
  info.native_window.handle = reinterpret_cast<void*>(0x1);
  info.width = 128;
  info.height = 128;

  auto context = skity::CreateGPUNativeWindowVK(
      static_cast<skity::GPUContext*>(nullptr), &info);
  EXPECT_EQ(context, nullptr);
}

TEST(GPUNativeWindowVKTest, CreateRejectsInvalidDescriptor) {
  skity::GPUNativeWindowInfoVK info = {};
  info.native_window.type = skity::VKNativeWindowType::kInvalid;
  info.width = 128;
  info.height = 128;

  auto gpu_context = CreateDebugGPUContext();
  ASSERT_NE(gpu_context, nullptr);

  auto context = skity::CreateGPUNativeWindowVK(gpu_context.get(), &info);
  EXPECT_EQ(context, nullptr);
}

TEST(GPUNativeWindowVKTest, CreateRejectsZeroSizedWindow) {
  auto gpu_context = CreateDebugGPUContext();
  ASSERT_NE(gpu_context, nullptr);

  skity::GPUNativeWindowInfoVK info = {};
  info.native_window.type = skity::VKNativeWindowType::kMetalLayer;
  info.native_window.handle = reinterpret_cast<void*>(0x1);
  info.width = 0;
  info.height = 128;

  auto native_window = skity::CreateGPUNativeWindowVK(gpu_context.get(), &info);
  EXPECT_EQ(native_window, nullptr);
}

}  // namespace
