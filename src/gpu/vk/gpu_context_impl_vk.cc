// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_context_impl_vk.hpp"

#include <vector>

#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/gpu/vk/vulkan_proc_table.hpp"
#include "src/logging.hpp"

// put VMA_IMPLEMENTATION here
#ifndef VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#endif
#ifndef VMA_STATIC_VULKAN_FUNCTIONS
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#endif
#ifndef VMA_DYNAMIC_VULKAN_FUNCTIONS
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#endif
#include <vk_mem_alloc.h>

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

void LogInstanceExtensions(const VulkanGlobalFns& global_fns) {
  if (global_fns.vkEnumerateInstanceExtensionProperties == nullptr) {
    LOGE("Failed to enumerate Vulkan instance extensions: loader is null");
    return;
  }

  uint32_t extension_count = 0;
  VkResult result = global_fns.vkEnumerateInstanceExtensionProperties(
      nullptr, &extension_count, nullptr);
  if (result != VK_SUCCESS) {
    LOGE("Failed to query Vulkan instance extension count: {}",
         static_cast<int32_t>(result));
    return;
  }

  LOGD("Enumerated {} Vulkan instance extension(s)", extension_count);
  if (extension_count == 0) {
    return;
  }

  std::vector<VkExtensionProperties> extensions(extension_count);
  result = global_fns.vkEnumerateInstanceExtensionProperties(
      nullptr, &extension_count, extensions.data());
  if (result != VK_SUCCESS) {
    LOGE("Failed to query Vulkan instance extensions: {}",
         static_cast<int32_t>(result));
    return;
  }

  for (const auto& extension : extensions) {
    LOGD("Vulkan instance extension: {} (spec version {})",
         extension.extensionName, extension.specVersion);
  }
}

}  // namespace

bool LoadVulkanGlobalFns(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                         VulkanGlobalFns* fns) {
  if (get_instance_proc_addr == nullptr || fns == nullptr) {
    LOGE("Failed to load Vulkan global procedures: invalid arguments");
    return false;
  }

  fns->vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
      get_instance_proc_addr(nullptr, "vkCreateInstance"));
  fns->vkEnumerateInstanceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
          get_instance_proc_addr(nullptr,
                                 "vkEnumerateInstanceExtensionProperties"));
  fns->vkEnumerateInstanceLayerProperties =
      reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
          get_instance_proc_addr(nullptr,
                                 "vkEnumerateInstanceLayerProperties"));
  fns->vkEnumerateInstanceVersion =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
          get_instance_proc_addr(nullptr, "vkEnumerateInstanceVersion"));

  if (fns->vkCreateInstance == nullptr) {
    LOGE("Failed to load Vulkan global procedures: vkCreateInstance is null");
    return false;
  }

  return true;
}

bool LoadVulkanInstanceFns(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                           VkInstance instance, VulkanInstanceFns* fns) {
  if (get_instance_proc_addr == nullptr || instance == VK_NULL_HANDLE ||
      fns == nullptr) {
    LOGE("Failed to load Vulkan instance procedures: invalid arguments");
    return false;
  }

  fns->vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
      get_instance_proc_addr(instance, "vkDestroyInstance"));
  fns->vkEnumeratePhysicalDevices =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          get_instance_proc_addr(instance, "vkEnumeratePhysicalDevices"));
  fns->vkGetPhysicalDeviceProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
          get_instance_proc_addr(instance, "vkGetPhysicalDeviceProperties"));
  fns->vkEnumerateDeviceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          get_instance_proc_addr(instance,
                                 "vkEnumerateDeviceExtensionProperties"));
  fns->vkGetPhysicalDeviceQueueFamilyProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
          get_instance_proc_addr(instance,
                                 "vkGetPhysicalDeviceQueueFamilyProperties"));
  fns->vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
      get_instance_proc_addr(instance, "vkCreateDevice"));
  fns->vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      get_instance_proc_addr(instance, "vkGetDeviceProcAddr"));
#if defined(SKITY_VK_DEBUG_RUNTIME)
  fns->vkCreateDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
          get_instance_proc_addr(instance, "vkCreateDebugUtilsMessengerEXT"));
  fns->vkDestroyDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
          get_instance_proc_addr(instance, "vkDestroyDebugUtilsMessengerEXT"));
#endif

  if (fns->vkDestroyInstance == nullptr ||
      fns->vkEnumeratePhysicalDevices == nullptr ||
      fns->vkGetPhysicalDeviceProperties == nullptr ||
      fns->vkEnumerateDeviceExtensionProperties == nullptr ||
      fns->vkGetPhysicalDeviceQueueFamilyProperties == nullptr ||
      fns->vkCreateDevice == nullptr || fns->vkGetDeviceProcAddr == nullptr) {
    LOGE("Failed to load Vulkan instance procedures for instance: {:p}",
         reinterpret_cast<void*>(instance));
    return false;
  }

  return true;
}

bool LoadVulkanDeviceFns(PFN_vkGetDeviceProcAddr get_device_proc_addr,
                         VkDevice device, VulkanDeviceFns* fns) {
  if (get_device_proc_addr == nullptr || device == VK_NULL_HANDLE ||
      fns == nullptr) {
    LOGE("Failed to load Vulkan device procedures: invalid arguments");
    return false;
  }

  fns->vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
      get_device_proc_addr(device, "vkDestroyDevice"));
  fns->vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
      get_device_proc_addr(device, "vkGetDeviceQueue"));
  fns->vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(
      get_device_proc_addr(device, "vkCreateShaderModule"));
  fns->vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(
      get_device_proc_addr(device, "vkDestroyShaderModule"));
  fns->vkSetDebugUtilsObjectNameEXT =
      reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
          get_device_proc_addr(device, "vkSetDebugUtilsObjectNameEXT"));

  if (fns->vkDestroyDevice == nullptr || fns->vkGetDeviceQueue == nullptr ||
      fns->vkCreateShaderModule == nullptr ||
      fns->vkDestroyShaderModule == nullptr) {
    LOGE("Failed to load Vulkan device procedures for device: {:p}",
         reinterpret_cast<void*>(device));
    return false;
  }

  return true;
}

bool CreateVkInstance(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                      VkInstance* instance, VulkanFunctionPointers* functions,
                      const VkInstanceCreateInfo* create_info,
                      const VkAllocationCallbacks* allocator) {
  if (instance == nullptr || get_instance_proc_addr == nullptr) {
    LOGE("Failed to create Vulkan instance: invalid arguments");
    return false;
  }

  *instance = VK_NULL_HANDLE;

  VulkanFunctionPointers local_functions = {};
  VulkanFunctionPointers* target_functions =
      functions == nullptr ? &local_functions : functions;
  target_functions->get_instance_proc_addr = get_instance_proc_addr;

  if (!LoadVulkanGlobalFns(get_instance_proc_addr, &target_functions->global)) {
    return false;
  }

  LogInstanceExtensions(target_functions->global);

  VkApplicationInfo app_info = {};
  VkInstanceCreateInfo instance_info = {};
  if (create_info == nullptr) {
    const uint32_t supported_version =
        ResolveInstanceApiVersion(target_functions->global);
    LOGD("Detected Vulkan instance version: {}.{}",
         VK_API_VERSION_MAJOR(supported_version),
         VK_API_VERSION_MINOR(supported_version));
    if (!IsAtLeastVulkan11(supported_version)) {
      LOGE("Vulkan 1.1 is required, but only {}.{} is available",
           VK_API_VERSION_MAJOR(supported_version),
           VK_API_VERSION_MINOR(supported_version));
      return false;
    }

    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "skity";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "skity";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = supported_version;

    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    create_info = &instance_info;
  }

  uint32_t api_version = VK_API_VERSION_1_0;
  if (create_info->pApplicationInfo != nullptr) {
    api_version = create_info->pApplicationInfo->apiVersion;
  }
  LOGD("Creating Vulkan instance, apiVersion: {}.{}",
       VK_API_VERSION_MAJOR(api_version), VK_API_VERSION_MINOR(api_version));
  VkResult result = target_functions->global.vkCreateInstance(
      create_info, allocator, instance);
  if (result != VK_SUCCESS || *instance == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan instance: result={}, instance={:p}",
         static_cast<int32_t>(result), reinterpret_cast<void*>(*instance));
    *instance = VK_NULL_HANDLE;
    return false;
  }

  if (!LoadVulkanInstanceFns(get_instance_proc_addr, *instance,
                             &target_functions->instance)) {
    target_functions->instance.vkDestroyInstance =
        reinterpret_cast<PFN_vkDestroyInstance>(
            get_instance_proc_addr(*instance, "vkDestroyInstance"));
    if (target_functions->instance.vkDestroyInstance != nullptr) {
      target_functions->instance.vkDestroyInstance(*instance, allocator);
    }
    LOGE("Failed to load Vulkan instance procedures after instance creation");
    *instance = VK_NULL_HANDLE;
    return false;
  }

  target_functions->get_device_proc_addr =
      target_functions->instance.vkGetDeviceProcAddr;
  LOGD("Created Vulkan instance: {:p}", reinterpret_cast<void*>(*instance));
  return true;
}

GPUContextVK::GPUContextVK(std::shared_ptr<VulkanContextState> state)
    : GPUContextImpl(GPUBackendType::kVulkan), state_(std::move(state)) {}

GPUContextVK::~GPUContextVK() = default;

std::unique_ptr<GPUSurface> GPUContextVK::CreateSurface(
    GPUSurfaceDescriptor* desc) {
  (void)desc;
  LOGW("GPUContextVK::CreateSurface is not implemented yet");
  return nullptr;
}

std::unique_ptr<GPUDevice> GPUContextVK::CreateGPUDevice() {
  return std::make_unique<GPUDeviceVK>(state_);
}

std::shared_ptr<GPUTexture> GPUContextVK::OnWrapTexture(
    GPUBackendTextureInfo* info, ReleaseCallback callback,
    ReleaseUserData user_data) {
  (void)info;
  (void)callback;
  (void)user_data;
  LOGW("GPUContextVK::OnWrapTexture is not implemented yet");
  return {};
}

std::unique_ptr<GPURenderTarget> GPUContextVK::OnCreateRenderTarget(
    const GPURenderTargetDescriptor& desc, std::shared_ptr<Texture> texture) {
  (void)desc;
  (void)texture;
  LOGW("GPUContextVK::OnCreateRenderTarget is not implemented yet");
  return {};
}

std::shared_ptr<Data> GPUContextVK::OnReadPixels(
    const std::shared_ptr<GPUTexture>& texture) const {
  (void)texture;
  LOGW("GPUContextVK::OnReadPixels is not implemented yet");
  return {};
}

std::unique_ptr<GPUContext> CreateGPUContextVK(const GPUContextInfoVK* info) {
  if (info == nullptr) {
    return nullptr;
  }

  LOGD("CreateGPUContextVK called with GPUContextInfoVK: {:p}",
       reinterpret_cast<const void*>(info));
  auto state = std::make_shared<VulkanContextState>();
  if (!state->Initialize(*info)) {
    return nullptr;
  }

  auto context = std::make_unique<GPUContextVK>(std::move(state));
  if (!context->Init()) {
    LOGE("Failed to initialize GPUContextVK device state");
    return nullptr;
  }

  return context;
}

std::unique_ptr<GPUContext> CreateGPUContextVK(
    PFN_vkGetInstanceProcAddr get_instance_proc_addr) {
  GPUContextInfoVK info = {};
  info.get_instance_proc_addr = get_instance_proc_addr;
  LOGD("CreateGPUContextVK called with vkGetInstanceProcAddr: {:p}",
       reinterpret_cast<void*>(get_instance_proc_addr));
  return CreateGPUContextVK(&info);
}

}  // namespace skity
