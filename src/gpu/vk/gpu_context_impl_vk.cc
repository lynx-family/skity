// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_context_impl_vk.hpp"

#include <cmath>
#include <skity/gpu/gpu_context_vk.hpp>
#include <string>
#include <vector>

#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/gpu_presenter_vk.hpp"
#include "src/gpu/vk/gpu_semaphore_vk.hpp"
#include "src/gpu/vk/gpu_surface_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/gpu/vk/vulkan_proc_table.hpp"
#include "src/logging.hpp"

#if defined(SKITY_ANDROID)
#include <android/hardware_buffer.h>

#include "src/gpu/vk/gpu_external_texture_ahb.hpp"
#endif

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
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-variable"
#endif
#include <vk_mem_alloc.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace skity {

namespace {

GPUTextureFormat ToGPUTextureFormat(VkFormat format) {
  switch (format) {
    case VK_FORMAT_R8_UNORM:
      return GPUTextureFormat::kR8Unorm;
    case VK_FORMAT_R8G8B8_UNORM:
      return GPUTextureFormat::kRGB8Unorm;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
      return GPUTextureFormat::kRGB565Unorm;
    case VK_FORMAT_R8G8B8A8_UNORM:
      return GPUTextureFormat::kRGBA8Unorm;
    case VK_FORMAT_B8G8R8A8_UNORM:
      return GPUTextureFormat::kBGRA8Unorm;
    case VK_FORMAT_S8_UINT:
      return GPUTextureFormat::kStencil8;
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return GPUTextureFormat::kDepth24Stencil8;
    default:
      return GPUTextureFormat::kInvalid;
  }
}

template <typename Proc>
Proc LoadInstanceProcByName(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                            VkInstance instance, const char* core_name,
                            const char* extension_name = nullptr) {
  auto proc =
      reinterpret_cast<Proc>(get_instance_proc_addr(instance, core_name));
  if (proc == nullptr && extension_name != nullptr) {
    proc = reinterpret_cast<Proc>(
        get_instance_proc_addr(instance, extension_name));
  }
  return proc;
}

template <typename Proc>
Proc LoadDeviceProcByName(PFN_vkGetDeviceProcAddr get_device_proc_addr,
                          VkDevice device, const char* core_name,
                          const char* extension_name = nullptr) {
  auto proc = reinterpret_cast<Proc>(get_device_proc_addr(device, core_name));
  if (proc == nullptr && extension_name != nullptr) {
    proc = reinterpret_cast<Proc>(get_device_proc_addr(device, extension_name));
  }
  return proc;
}

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

bool ContainsExtension(const std::vector<VkExtensionProperties>& extensions,
                       const char* extension_name) {
  if (extension_name == nullptr) {
    return false;
  }

  for (const auto& extension : extensions) {
    if (std::string(extension.extensionName) == extension_name) {
      return true;
    }
  }

  return false;
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

GPUTextureUsageMask ToGPUTextureUsageMask(VkImageUsageFlags image_usage) {
  GPUTextureUsageMask usage = 0;

  if ((image_usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0) {
    usage |=
        static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);
  }

  if ((image_usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0) {
    usage |= static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopySrc);
  }

  if ((image_usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0) {
    usage |= static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopyDst);
  }

  if ((image_usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0) {
    usage |= static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
  }

  if ((image_usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) {
    usage |= static_cast<GPUTextureUsageMask>(GPUTextureUsage::kStorageBinding);
  }

  return usage;
}

bool QueryInstanceExtensions(const VulkanGlobalFns& global_fns,
                             std::vector<VkExtensionProperties>* extensions) {
  if (extensions == nullptr) {
    return false;
  }

  if (global_fns.vkEnumerateInstanceExtensionProperties == nullptr) {
    LOGE("Failed to enumerate Vulkan instance extensions: loader is null");
    return false;
  }

  uint32_t extension_count = 0;
  VkResult result = global_fns.vkEnumerateInstanceExtensionProperties(
      nullptr, &extension_count, nullptr);
  if (result != VK_SUCCESS) {
    LOGE("Failed to query Vulkan instance extension count: {}",
         static_cast<int32_t>(result));
    return false;
  }

  extensions->clear();
  if (extension_count == 0) {
    return true;
  }

  extensions->resize(extension_count);
  result = global_fns.vkEnumerateInstanceExtensionProperties(
      nullptr, &extension_count, extensions->data());
  if (result != VK_SUCCESS) {
    LOGE("Failed to query Vulkan instance extensions: {}",
         static_cast<int32_t>(result));
    extensions->clear();
    return false;
  }

  extensions->resize(extension_count);
  return true;
}

void LogInstanceExtensions(
    const std::vector<VkExtensionProperties>& extensions) {
#ifdef SKITY_RELEASE
  (void)extensions;
#else
  LOGD("Enumerated {} Vulkan instance extension(s)", extensions.size());
  for (const auto& extension : extensions) {
    LOGD("Vulkan instance extension: {} (spec version {})",
         extension.extensionName, extension.specVersion);
  }
#endif
}

VkImageAspectFlags GetImageAspectMaskForReadPixels(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kStencil8:
      return VK_IMAGE_ASPECT_STENCIL_BIT;
    case GPUTextureFormat::kDepth24Stencil8:
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case GPUTextureFormat::kInvalid:
      return 0;
    default:
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

VkAccessFlags AccessMaskForLayout(VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
      return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
             VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    default:
      return 0;
  }
}

VkPipelineStageFlags StageMaskForLayout(VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
      return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    default:
      return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }
}

void TransitionImageLayoutForReadPixels(const VulkanContextState& state,
                                        VkCommandBuffer command_buffer,
                                        GPUTextureVK& texture,
                                        VkImageLayout new_layout) {
  const VkImageLayout old_layout = texture.GetCurrentLayout();
  if (old_layout == new_layout) {
    return;
  }

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = texture.GetImage();
  barrier.subresourceRange.aspectMask =
      GetImageAspectMaskForReadPixels(texture.GetDescriptor().format);
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = texture.GetDescriptor().mip_level_count;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = AccessMaskForLayout(old_layout);
  barrier.dstAccessMask = AccessMaskForLayout(new_layout);

  state.DeviceFns().vkCmdPipelineBarrier(
      command_buffer, StageMaskForLayout(old_layout),
      StageMaskForLayout(new_layout), 0, 0, nullptr, 0, nullptr, 1, &barrier);

  texture.SetCurrentLayout(new_layout);
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
  fns->vkGetPhysicalDeviceFormatProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
          get_instance_proc_addr(instance,
                                 "vkGetPhysicalDeviceFormatProperties"));
  fns->vkGetPhysicalDeviceMemoryProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
          get_instance_proc_addr(instance,
                                 "vkGetPhysicalDeviceMemoryProperties"));
  fns->vkGetPhysicalDeviceImageFormatProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(
          get_instance_proc_addr(instance,
                                 "vkGetPhysicalDeviceImageFormatProperties"));
  fns->vkGetPhysicalDeviceFeatures2 =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
          get_instance_proc_addr(instance, "vkGetPhysicalDeviceFeatures2"));
  fns->vkGetPhysicalDeviceMemoryProperties2KHR =
      LoadInstanceProcByName<PFN_vkGetPhysicalDeviceMemoryProperties2KHR>(
          get_instance_proc_addr, instance,
          "vkGetPhysicalDeviceMemoryProperties2",
          "vkGetPhysicalDeviceMemoryProperties2KHR");
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
      fns->vkGetPhysicalDeviceFormatProperties == nullptr ||
      fns->vkGetPhysicalDeviceMemoryProperties == nullptr ||
      fns->vkGetPhysicalDeviceImageFormatProperties == nullptr ||
      fns->vkGetPhysicalDeviceFeatures2 == nullptr ||
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
  fns->vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(
      get_device_proc_addr(device, "vkDeviceWaitIdle"));
  fns->vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
      get_device_proc_addr(device, "vkGetDeviceQueue"));
  fns->vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
      get_device_proc_addr(device, "vkQueueSubmit"));
  fns->vkQueueSubmit2 = reinterpret_cast<PFN_vkQueueSubmit2>(
      get_device_proc_addr(device, "vkQueueSubmit2"));
  fns->vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(
      get_device_proc_addr(device, "vkCreateCommandPool"));
  fns->vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(
      get_device_proc_addr(device, "vkDestroyCommandPool"));
  fns->vkAllocateCommandBuffers =
      reinterpret_cast<PFN_vkAllocateCommandBuffers>(
          get_device_proc_addr(device, "vkAllocateCommandBuffers"));
  fns->vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(
      get_device_proc_addr(device, "vkBeginCommandBuffer"));
  fns->vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(
      get_device_proc_addr(device, "vkEndCommandBuffer"));
  fns->vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(
      get_device_proc_addr(device, "vkCreateFence"));
  fns->vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(
      get_device_proc_addr(device, "vkDestroyFence"));
  fns->vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(
      get_device_proc_addr(device, "vkGetFenceStatus"));
  fns->vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(
      get_device_proc_addr(device, "vkWaitForFences"));
  fns->vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(
      get_device_proc_addr(device, "vkAllocateMemory"));
  fns->vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(
      get_device_proc_addr(device, "vkFreeMemory"));
  fns->vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(
      get_device_proc_addr(device, "vkMapMemory"));
  fns->vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(
      get_device_proc_addr(device, "vkUnmapMemory"));
  fns->vkFlushMappedMemoryRanges =
      reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(
          get_device_proc_addr(device, "vkFlushMappedMemoryRanges"));
  fns->vkInvalidateMappedMemoryRanges =
      reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(
          get_device_proc_addr(device, "vkInvalidateMappedMemoryRanges"));
  fns->vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(
      get_device_proc_addr(device, "vkBindBufferMemory"));
  fns->vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(
      get_device_proc_addr(device, "vkBindImageMemory"));
  fns->vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(
      get_device_proc_addr(device, "vkCreateSemaphore"));
  fns->vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(
      get_device_proc_addr(device, "vkDestroySemaphore"));
  fns->vkGetBufferMemoryRequirements =
      reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
          get_device_proc_addr(device, "vkGetBufferMemoryRequirements"));
  fns->vkGetImageMemoryRequirements =
      reinterpret_cast<PFN_vkGetImageMemoryRequirements>(
          get_device_proc_addr(device, "vkGetImageMemoryRequirements"));
  fns->vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(
      get_device_proc_addr(device, "vkCreateBuffer"));
  fns->vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(
      get_device_proc_addr(device, "vkDestroyBuffer"));
  fns->vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(
      get_device_proc_addr(device, "vkCreateImage"));
  fns->vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(
      get_device_proc_addr(device, "vkDestroyImage"));
  fns->vkGetBufferMemoryRequirements2KHR =
      LoadDeviceProcByName<PFN_vkGetBufferMemoryRequirements2KHR>(
          get_device_proc_addr, device, "vkGetBufferMemoryRequirements2",
          "vkGetBufferMemoryRequirements2KHR");
  fns->vkGetImageMemoryRequirements2KHR =
      LoadDeviceProcByName<PFN_vkGetImageMemoryRequirements2KHR>(
          get_device_proc_addr, device, "vkGetImageMemoryRequirements2",
          "vkGetImageMemoryRequirements2KHR");
  fns->vkBindBufferMemory2KHR =
      LoadDeviceProcByName<PFN_vkBindBufferMemory2KHR>(
          get_device_proc_addr, device, "vkBindBufferMemory2",
          "vkBindBufferMemory2KHR");
  fns->vkBindImageMemory2KHR = LoadDeviceProcByName<PFN_vkBindImageMemory2KHR>(
      get_device_proc_addr, device, "vkBindImageMemory2",
      "vkBindImageMemory2KHR");
  fns->vkGetDeviceBufferMemoryRequirements =
      LoadDeviceProcByName<PFN_vkGetDeviceBufferMemoryRequirementsKHR>(
          get_device_proc_addr, device, "vkGetDeviceBufferMemoryRequirements",
          "vkGetDeviceBufferMemoryRequirementsKHR");
  fns->vkGetDeviceImageMemoryRequirements =
      LoadDeviceProcByName<PFN_vkGetDeviceImageMemoryRequirementsKHR>(
          get_device_proc_addr, device, "vkGetDeviceImageMemoryRequirements",
          "vkGetDeviceImageMemoryRequirementsKHR");
  fns->vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(
      get_device_proc_addr(device, "vkCmdCopyBuffer"));
  fns->vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(
      get_device_proc_addr(device, "vkCreateFramebuffer"));
  fns->vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(
      get_device_proc_addr(device, "vkDestroyFramebuffer"));
  fns->vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(
      get_device_proc_addr(device, "vkCreateRenderPass"));
  fns->vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(
      get_device_proc_addr(device, "vkDestroyRenderPass"));
  fns->vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(
      get_device_proc_addr(device, "vkCreateImageView"));
  fns->vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(
      get_device_proc_addr(device, "vkDestroyImageView"));
  fns->vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(
      get_device_proc_addr(device, "vkCmdBeginRenderPass"));
  fns->vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(
      get_device_proc_addr(device, "vkCmdEndRenderPass"));
  fns->vkCmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(
      get_device_proc_addr(device, "vkCmdCopyImage"));
  fns->vkCmdBlitImage = reinterpret_cast<PFN_vkCmdBlitImage>(
      get_device_proc_addr(device, "vkCmdBlitImage"));
  fns->vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(
      get_device_proc_addr(device, "vkCmdCopyBufferToImage"));
  fns->vkCmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(
      get_device_proc_addr(device, "vkCmdCopyImageToBuffer"));
  fns->vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
      get_device_proc_addr(device, "vkCmdPipelineBarrier"));
  fns->vkCmdBeginRendering = LoadDeviceProcByName<PFN_vkCmdBeginRendering>(
      get_device_proc_addr, device, "vkCmdBeginRendering",
      "vkCmdBeginRenderingKHR");
  fns->vkCmdEndRendering = LoadDeviceProcByName<PFN_vkCmdEndRendering>(
      get_device_proc_addr, device, "vkCmdEndRendering",
      "vkCmdEndRenderingKHR");
  fns->vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(
      get_device_proc_addr(device, "vkCreateShaderModule"));
  fns->vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(
      get_device_proc_addr(device, "vkDestroyShaderModule"));
  fns->vkCreateSampler = reinterpret_cast<PFN_vkCreateSampler>(
      get_device_proc_addr(device, "vkCreateSampler"));
  fns->vkDestroySampler = reinterpret_cast<PFN_vkDestroySampler>(
      get_device_proc_addr(device, "vkDestroySampler"));
  fns->vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(
      get_device_proc_addr(device, "vkCreateDescriptorPool"));
  fns->vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(
      get_device_proc_addr(device, "vkDestroyDescriptorPool"));
  fns->vkAllocateDescriptorSets =
      reinterpret_cast<PFN_vkAllocateDescriptorSets>(
          get_device_proc_addr(device, "vkAllocateDescriptorSets"));
  fns->vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(
      get_device_proc_addr(device, "vkUpdateDescriptorSets"));
  fns->vkCreateDescriptorSetLayout =
      reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
          get_device_proc_addr(device, "vkCreateDescriptorSetLayout"));
  fns->vkDestroyDescriptorSetLayout =
      reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(
          get_device_proc_addr(device, "vkDestroyDescriptorSetLayout"));
  fns->vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(
      get_device_proc_addr(device, "vkCreatePipelineLayout"));
  fns->vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(
      get_device_proc_addr(device, "vkDestroyPipelineLayout"));
  fns->vkCreatePipelineCache = reinterpret_cast<PFN_vkCreatePipelineCache>(
      get_device_proc_addr(device, "vkCreatePipelineCache"));
  fns->vkDestroyPipelineCache = reinterpret_cast<PFN_vkDestroyPipelineCache>(
      get_device_proc_addr(device, "vkDestroyPipelineCache"));
  fns->vkCreateGraphicsPipelines =
      reinterpret_cast<PFN_vkCreateGraphicsPipelines>(
          get_device_proc_addr(device, "vkCreateGraphicsPipelines"));
  fns->vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(
      get_device_proc_addr(device, "vkDestroyPipeline"));
  fns->vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(
      get_device_proc_addr(device, "vkCmdBindPipeline"));
  fns->vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(
      get_device_proc_addr(device, "vkCmdBindDescriptorSets"));
  fns->vkCmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(
      get_device_proc_addr(device, "vkCmdBindVertexBuffers"));
  fns->vkCmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(
      get_device_proc_addr(device, "vkCmdBindIndexBuffer"));
  fns->vkCmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(
      get_device_proc_addr(device, "vkCmdDrawIndexed"));
  fns->vkCmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(
      get_device_proc_addr(device, "vkCmdSetViewport"));
  fns->vkCmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(
      get_device_proc_addr(device, "vkCmdSetScissor"));
  fns->vkCmdSetStencilCompareMask =
      reinterpret_cast<PFN_vkCmdSetStencilCompareMask>(
          get_device_proc_addr(device, "vkCmdSetStencilCompareMask"));
  fns->vkCmdSetStencilWriteMask =
      reinterpret_cast<PFN_vkCmdSetStencilWriteMask>(
          get_device_proc_addr(device, "vkCmdSetStencilWriteMask"));
  fns->vkCmdSetStencilReference =
      reinterpret_cast<PFN_vkCmdSetStencilReference>(
          get_device_proc_addr(device, "vkCmdSetStencilReference"));
  fns->vkSetDebugUtilsObjectNameEXT =
      reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
          get_device_proc_addr(device, "vkSetDebugUtilsObjectNameEXT"));
#if defined(SKITY_ANDROID)
  fns->vkImportSemaphoreFdKHR = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(
      get_device_proc_addr(device, "vkImportSemaphoreFdKHR"));
  fns->vkGetAndroidHardwareBufferPropertiesANDROID =
      reinterpret_cast<PFN_vkGetAndroidHardwareBufferPropertiesANDROID>(
          get_device_proc_addr(device,
                               "vkGetAndroidHardwareBufferPropertiesANDROID"));
#endif
#if defined(SKITY_VK_DEBUG_RUNTIME)
  fns->vkCmdBeginDebugUtilsLabelEXT =
      reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
          get_device_proc_addr(device, "vkCmdBeginDebugUtilsLabelEXT"));
  fns->vkCmdEndDebugUtilsLabelEXT =
      reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
          get_device_proc_addr(device, "vkCmdEndDebugUtilsLabelEXT"));
  fns->vkCmdInsertDebugUtilsLabelEXT =
      reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
          get_device_proc_addr(device, "vkCmdInsertDebugUtilsLabelEXT"));
#endif

  if (fns->vkDestroyDevice == nullptr || fns->vkDeviceWaitIdle == nullptr ||
      fns->vkGetDeviceQueue == nullptr || fns->vkQueueSubmit == nullptr ||
      fns->vkCreateCommandPool == nullptr ||
      fns->vkDestroyCommandPool == nullptr ||
      fns->vkAllocateCommandBuffers == nullptr ||
      fns->vkBeginCommandBuffer == nullptr ||
      fns->vkEndCommandBuffer == nullptr || fns->vkCreateFence == nullptr ||
      fns->vkDestroyFence == nullptr || fns->vkGetFenceStatus == nullptr ||
      fns->vkWaitForFences == nullptr || fns->vkCmdCopyBuffer == nullptr ||
      fns->vkAllocateMemory == nullptr || fns->vkFreeMemory == nullptr ||
      fns->vkMapMemory == nullptr || fns->vkUnmapMemory == nullptr ||
      fns->vkFlushMappedMemoryRanges == nullptr ||
      fns->vkInvalidateMappedMemoryRanges == nullptr ||
      fns->vkBindBufferMemory == nullptr || fns->vkBindImageMemory == nullptr ||
      fns->vkCreateSemaphore == nullptr || fns->vkDestroySemaphore == nullptr ||
      fns->vkGetBufferMemoryRequirements == nullptr ||
      fns->vkGetImageMemoryRequirements == nullptr ||
      fns->vkCreateBuffer == nullptr || fns->vkDestroyBuffer == nullptr ||
      fns->vkCreateImage == nullptr || fns->vkDestroyImage == nullptr ||
      fns->vkCreateFramebuffer == nullptr ||
      fns->vkDestroyFramebuffer == nullptr ||
      fns->vkCreateRenderPass == nullptr ||
      fns->vkDestroyRenderPass == nullptr ||
      fns->vkCreateImageView == nullptr || fns->vkDestroyImageView == nullptr ||
      fns->vkCmdBeginRenderPass == nullptr ||
      fns->vkCmdEndRenderPass == nullptr || fns->vkCmdBlitImage == nullptr ||
      fns->vkCmdCopyBufferToImage == nullptr ||
      fns->vkCmdPipelineBarrier == nullptr ||
      fns->vkCreateShaderModule == nullptr ||
      fns->vkDestroyShaderModule == nullptr ||
      fns->vkCreateSampler == nullptr || fns->vkDestroySampler == nullptr ||
      fns->vkCreateDescriptorPool == nullptr ||
      fns->vkDestroyDescriptorPool == nullptr ||
      fns->vkAllocateDescriptorSets == nullptr ||
      fns->vkUpdateDescriptorSets == nullptr ||
      fns->vkCreateDescriptorSetLayout == nullptr ||
      fns->vkDestroyDescriptorSetLayout == nullptr ||
      fns->vkCreatePipelineLayout == nullptr ||
      fns->vkDestroyPipelineLayout == nullptr ||
      fns->vkCreatePipelineCache == nullptr ||
      fns->vkDestroyPipelineCache == nullptr ||
      fns->vkCreateGraphicsPipelines == nullptr ||
      fns->vkDestroyPipeline == nullptr || fns->vkCmdBindPipeline == nullptr ||
      fns->vkCmdBindDescriptorSets == nullptr ||
      fns->vkCmdBindVertexBuffers == nullptr ||
      fns->vkCmdBindIndexBuffer == nullptr ||
      fns->vkCmdDrawIndexed == nullptr || fns->vkCmdSetViewport == nullptr ||
      fns->vkCmdSetScissor == nullptr ||
      fns->vkCmdSetStencilCompareMask == nullptr ||
      fns->vkCmdSetStencilWriteMask == nullptr ||
      fns->vkCmdSetStencilReference == nullptr) {
    LOGE("Failed to load Vulkan device procedures for device: {:p}",
         reinterpret_cast<void*>(device));
    return false;
  }

  return true;
}  // NOLINT(readability/fn_size)

void EnablePortabilityEnumerationIfAvailable(
    const std::vector<VkExtensionProperties>& available_extensions,
    std::vector<std::string>* enabled_extensions,
    VkInstanceCreateFlags* instance_flags) {
  if (enabled_extensions == nullptr || instance_flags == nullptr) {
    return;
  }

  if (!ContainsExtension(available_extensions,
                         VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) ||
      ContainsExtension(*enabled_extensions,
                        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    return;
  }

  enabled_extensions->emplace_back(
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  *instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
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

  std::vector<VkExtensionProperties> available_extensions;
  if (!QueryInstanceExtensions(target_functions->global,
                               &available_extensions)) {
    return false;
  }
  LogInstanceExtensions(available_extensions);

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

  VkInstanceCreateInfo effective_create_info = *create_info;
  std::vector<std::string> enabled_extensions;
  enabled_extensions.reserve(create_info->enabledExtensionCount + 1);
  for (uint32_t i = 0; i < create_info->enabledExtensionCount; ++i) {
    if (create_info->ppEnabledExtensionNames[i] != nullptr) {
      enabled_extensions.emplace_back(create_info->ppEnabledExtensionNames[i]);
    }
  }
  EnablePortabilityEnumerationIfAvailable(
      available_extensions, &enabled_extensions, &effective_create_info.flags);
  std::vector<const char*> enabled_extension_ptrs;
  enabled_extension_ptrs.reserve(enabled_extensions.size());
  for (const auto& extension : enabled_extensions) {
    enabled_extension_ptrs.push_back(extension.c_str());
  }
  effective_create_info.enabledExtensionCount =
      static_cast<uint32_t>(enabled_extension_ptrs.size());
  effective_create_info.ppEnabledExtensionNames =
      enabled_extension_ptrs.empty() ? nullptr : enabled_extension_ptrs.data();
  create_info = &effective_create_info;

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

GPUContextVK::~GPUContextVK() {
  if (state_ != nullptr) {
    state_->CollectPendingSubmissions(true);
  }
  ResetOwnedResources();
}

std::unique_ptr<GPUSurface> GPUContextVK::CreateSurface(
    GPUSurfaceDescriptor* desc) {
  if (desc == nullptr || desc->backend != GPUBackendType::kVulkan) {
    return nullptr;
  }

  auto* vk_desc = static_cast<GPUSurfaceDescriptorVK*>(desc);
  if (vk_desc->surface_type != VKSurfaceType::kTexture &&
      vk_desc->surface_type != VKSurfaceType::kSwapchainImage) {
    LOGW(
        "GPUContextVK::CreateSurface only supports texture-backed Vulkan "
        "surfaces for now");
    return nullptr;
  }

  const uint32_t target_width = static_cast<uint32_t>(
      std::floor(static_cast<float>(vk_desc->width) * vk_desc->content_scale));
  const uint32_t target_height = static_cast<uint32_t>(
      std::floor(static_cast<float>(vk_desc->height) * vk_desc->content_scale));

  if (vk_desc->image == VK_NULL_HANDLE ||
      vk_desc->image_view == VK_NULL_HANDLE ||
      vk_desc->format == VK_FORMAT_UNDEFINED || vk_desc->width == 0 ||
      vk_desc->height == 0 || target_width == 0 || target_height == 0) {
    LOGE("Failed to create Vulkan surface: invalid descriptor");
    return nullptr;
  }

  GPUTextureDescriptor texture_desc = {};
  texture_desc.width = target_width;
  texture_desc.height = target_height;
  texture_desc.mip_level_count = 1;
  texture_desc.sample_count = 1;
  texture_desc.format = ToGPUTextureFormat(vk_desc->format);
  texture_desc.usage = ToGPUTextureUsageMask(vk_desc->image_usage);
  texture_desc.storage_mode = GPUTextureStorageMode::kPrivate;

  if (texture_desc.format == GPUTextureFormat::kInvalid) {
    LOGE("Failed to create Vulkan surface: unsupported VkFormat {}",
         static_cast<int32_t>(vk_desc->format));
    return nullptr;
  }

  auto texture = GPUTextureVK::Wrap(
      state_, texture_desc, vk_desc->image, vk_desc->image_view,
      vk_desc->initial_layout, vk_desc->final_layout, vk_desc->format,
      vk_desc->owns_image, vk_desc->owns_image_view);
  if (texture == nullptr) {
    return nullptr;
  }

  return std::make_unique<GPUSurfaceVK>(
      *desc, this, std::move(texture), texture_desc.format, vk_desc->sync_info);
}

std::unique_ptr<GPUPresenter> GPUContextVK::CreatePresenter(
    GPUPresenterDescriptor* desc) {
  if (desc == nullptr || desc->backend != GPUBackendType::kVulkan) {
    return nullptr;
  }

  auto* vk_desc = static_cast<GPUPresenterDescriptorVK*>(desc);
  auto presenter = std::make_unique<GPUPresenterVK>(this, state_, *vk_desc);
  if (!presenter->Init()) {
    return nullptr;
  }
  return presenter;
}

std::shared_ptr<GPUSemaphore> GPUContextVK::CreateSemaphore() {
  VkSemaphoreCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkSemaphore sem = VK_NULL_HANDLE;
  auto result = state_->DeviceFns().vkCreateSemaphore(
      state_->GetLogicalDevice(), &create_info, nullptr, &sem);
  if (result != VK_SUCCESS) {
    LOGE("GPUContextVK::CreateSemaphore: vkCreateSemaphore failed: {}",
         static_cast<int32_t>(result));
    return nullptr;
  }

  return std::shared_ptr<GPUSemaphoreVK>(new GPUSemaphoreVK(state_, sem));
}

void GPUContextVK::ImportSemaphore(GPUSemaphore* semaphore,
                                   const GPUSemaphoreImportInfo& info) {
  if (semaphore == nullptr || info.backend != GPUBackendType::kVulkan) {
    return;
  }

  auto* vk_sem = static_cast<GPUSemaphoreVK*>(semaphore);
  const auto& vk_info = static_cast<const GPUSemaphoreImportInfoVK&>(info);

#if defined(SKITY_ANDROID)
  if (state_->DeviceFns().vkImportSemaphoreFdKHR == nullptr) {
    LOGE("GPUContextVK::ImportSemaphore: vkImportSemaphoreFdKHR not available");
    return;
  }

  VkImportSemaphoreFdInfoKHR import_info = {};
  import_info.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
  import_info.semaphore = vk_sem->GetVkSemaphore();
  import_info.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;
  import_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
  import_info.fd = vk_info.sync_fd;  // ownership transferred

  VkResult result = state_->DeviceFns().vkImportSemaphoreFdKHR(
      state_->GetLogicalDevice(), &import_info);
  if (result != VK_SUCCESS) {
    LOGE("GPUContextVK::ImportSemaphore: vkImportSemaphoreFdKHR failed: {}",
         static_cast<int32_t>(result));
  }
#else
  (void)vk_sem;
  (void)vk_info;
  LOGE("GPUContextVK::ImportSemaphore: not supported on this platform");
#endif
}

std::unique_ptr<GPUDevice> GPUContextVK::CreateGPUDevice() {
  return std::make_unique<GPUDeviceVK>(state_);
}

std::shared_ptr<GPUTexture> GPUContextVK::OnWrapTexture(
    GPUBackendTextureInfo* info, ReleaseCallback callback,
    ReleaseUserData user_data) {
  if (info->backend != GPUBackendType::kVulkan) {
    return nullptr;
  }

  auto* vk_info = static_cast<GPUBackendTextureInfoVK*>(info);

#if defined(SKITY_ANDROID)
  // Walk the ext chain to find AHB extension
  GPUBackendTextureExtInfoAHB* ahb_ext = nullptr;
  for (auto* e = vk_info->ext; e != nullptr; e = e->next) {
    if (e->type == GPUBackendTextureExtType::kAndroidHardwareBuffer) {
      ahb_ext = static_cast<GPUBackendTextureExtInfoAHB*>(e);
      break;
    }
  }
  if (ahb_ext && ahb_ext->hardware_buffer) {
    return OnWrapAHardwareBuffer(ahb_ext->hardware_buffer, vk_info->width,
                                 vk_info->height, callback, user_data);
  }
#endif

  if (vk_info->image == VK_NULL_HANDLE ||
      vk_info->image_view == VK_NULL_HANDLE ||
      vk_info->vk_format == VK_FORMAT_UNDEFINED || vk_info->width == 0 ||
      vk_info->height == 0) {
    LOGE("GPUContextVK::OnWrapTexture: invalid descriptor");
    return nullptr;
  }

  GPUTextureFormat gpu_format = ToGPUTextureFormat(vk_info->vk_format);
  if (gpu_format == GPUTextureFormat::kInvalid) {
    LOGE("GPUContextVK::OnWrapTexture: unsupported VkFormat {}",
         static_cast<int32_t>(vk_info->vk_format));
    return nullptr;
  }

  GPUTextureDescriptor desc = {};
  desc.width = vk_info->width;
  desc.height = vk_info->height;
  desc.mip_level_count = 1;
  desc.sample_count = 1;
  desc.format = gpu_format;
  desc.usage = ToGPUTextureUsageMask(vk_info->image_usage);
  desc.storage_mode = GPUTextureStorageMode::kPrivate;

  auto texture = GPUTextureVK::Wrap(
      state_, desc, vk_info->image, vk_info->image_view,
      vk_info->initial_layout, vk_info->final_layout, vk_info->vk_format,
      vk_info->owns_image, vk_info->owns_image_view);
  if (texture == nullptr) {
    return nullptr;
  }

  texture->SetRelease(callback, user_data);
  return texture;
}

std::unique_ptr<GPURenderTarget> GPUContextVK::OnCreateRenderTarget(
    const GPURenderTargetDescriptor& desc, std::shared_ptr<Texture> texture) {
  if (texture == nullptr) {
    return nullptr;
  }

  auto gpu_texture = texture->GetGPUTexture();
  if (gpu_texture == nullptr) {
    return nullptr;
  }

  auto* vk_texture = GPUTextureVK::Cast(gpu_texture.get());
  if (vk_texture == nullptr || !vk_texture->IsValid()) {
    return nullptr;
  }

  GPUTextureDescriptor texture_desc = {};
  texture_desc.width = desc.width;
  texture_desc.height = desc.height;
  texture_desc.mip_level_count = 1;
  texture_desc.sample_count = desc.sample_count;
  texture_desc.format = ToGPUTextureFormat(vk_texture->GetVkFormat());
  texture_desc.usage = ToGPUTextureUsageMask(
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
  texture_desc.storage_mode = GPUTextureStorageMode::kPrivate;

  if (texture_desc.format == GPUTextureFormat::kInvalid) {
    LOGE("GPUContextVK::OnCreateRenderTarget: unsupported VkFormat {}",
         static_cast<int32_t>(vk_texture->GetVkFormat()));
    return nullptr;
  }

  // Wrap the existing Vulkan handles without taking ownership. The original
  // GPUTextureVK (managed by TextureManager) owns the handles and controls
  // their lifecycle.
  auto wrapped_texture = GPUTextureVK::Wrap(
      state_, texture_desc, vk_texture->GetImage(), vk_texture->GetImageView(),
      vk_texture->GetCurrentLayout(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      vk_texture->GetVkFormat(), false, false);
  if (wrapped_texture == nullptr) {
    return nullptr;
  }

  GPUSurfaceDescriptorVK surface_desc = {};
  surface_desc.backend = GPUBackendType::kVulkan;
  surface_desc.width = desc.width;
  surface_desc.height = desc.height;
  surface_desc.content_scale = 1.0f;
  surface_desc.sample_count = desc.sample_count;
  surface_desc.surface_type = VKSurfaceType::kTexture;

  auto surface = std::make_unique<GPUSurfaceVK>(surface_desc, this,
                                                std::move(wrapped_texture),
                                                texture_desc.format, nullptr);

  return std::make_unique<GPURenderTarget>(std::move(surface), texture);
}

std::shared_ptr<Data> GPUContextVK::OnReadPixels(
    const std::shared_ptr<GPUTexture>& texture) const {
  auto* vk_texture = GPUTextureVK::Cast(texture.get());
  if (vk_texture == nullptr || !vk_texture->IsValid()) {
    return nullptr;
  }

  const auto& desc = vk_texture->GetDescriptor();
  const size_t bytes_per_pixel = GetTextureFormatBytesPerPixel(desc.format);
  if (bytes_per_pixel == 0) {
    LOGE("GPUContextVK::OnReadPixels: unsupported texture format");
    return nullptr;
  }

  const size_t total_bytes =
      static_cast<size_t>(desc.width) * desc.height * bytes_per_pixel;

  // Create a host-visible staging buffer as the copy destination.
  auto staging_buffer = std::make_unique<GPUBufferVK>(
      0u, state_, GPUBufferVKMemoryType::kHostVisible);
  if (!staging_buffer->ResizeIfNeeded(total_bytes)) {
    LOGE("GPUContextVK::OnReadPixels: failed to create staging buffer");
    return nullptr;
  }

  // Record and submit an image-to-buffer copy command.
  auto command_buffer = std::make_shared<GPUCommandBufferVK>(state_);
  if (!command_buffer->Init()) {
    return nullptr;
  }

  command_buffer->SetLabel("VkReadPixels");

  // Transition image to TRANSFER_SRC_OPTIMAL for reading.
  TransitionImageLayoutForReadPixels(
      *state_, command_buffer->GetCommandBuffer(), *vk_texture,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  VkBufferImageCopy copy_region = {};
  copy_region.bufferOffset = 0;
  copy_region.bufferRowLength = 0;
  copy_region.bufferImageHeight = 0;
  copy_region.imageSubresource.aspectMask =
      GetImageAspectMaskForReadPixels(desc.format);
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageOffset = {0, 0, 0};
  copy_region.imageExtent = {desc.width, desc.height, 1};

  state_->DeviceFns().vkCmdCopyImageToBuffer(
      command_buffer->GetCommandBuffer(), vk_texture->GetImage(),
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer->GetBuffer(), 1,
      &copy_region);

  // Transition image back to its preferred layout.
  TransitionImageLayoutForReadPixels(
      *state_, command_buffer->GetCommandBuffer(), *vk_texture,
      vk_texture->GetPreferredLayout());

  // Keep the command buffer alive through the cleanup action so the staging
  // buffer remains valid after CollectPendingSubmissions destroys the pending
  // submission objects. The shared_ptr prevents early destruction.
  command_buffer->RecordCleanupAction([command_buffer] {});

  if (!command_buffer->Submit()) {
    LOGE("GPUContextVK::OnReadPixels: failed to submit command buffer");
    return nullptr;
  }

  // Wait for GPU to finish before reading back.
  state_->CollectPendingSubmissions(true);

  // Copy from mapped staging buffer into a Data object.
  void* mapped = staging_buffer->GetMappedData();
  if (mapped == nullptr) {
    LOGE("GPUContextVK::OnReadPixels: staging buffer is not mapped");
    return nullptr;
  }

  return Data::MakeWithCopy(mapped, total_bytes);
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

#if defined(SKITY_ANDROID)
std::shared_ptr<GPUTexture> GPUContextVK::OnWrapAHardwareBuffer(
    ::AHardwareBuffer* ahb, uint32_t width, uint32_t height,
    ReleaseCallback callback, ReleaseUserData user_data) {
  if (ahb == nullptr) {
    LOGE("GPUContextVK::OnWrapAHardwareBuffer: null AHardwareBuffer");
    return {};
  }

  // Check AHB extension is available
  if (!state_->HasEnabledDeviceExtension(
          VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME)) {
    LOGE(
        "GPUContextVK::OnWrapAHardwareBuffer: "
        "VK_ANDROID_external_memory_android_hardware_buffer not enabled");
    return {};
  }

  const auto& fns = state_->DeviceFns();
  if (fns.vkGetAndroidHardwareBufferPropertiesANDROID == nullptr) {
    LOGE(
        "GPUContextVK::OnWrapAHardwareBuffer: "
        "vkGetAndroidHardwareBufferPropertiesANDROID not loaded");
    return {};
  }

  VkDevice device = state_->GetLogicalDevice();

  // Step 1: Describe AHardwareBuffer
  if (!state_->IsAHardwareBufferAvailable()) {
    LOGE(
        "GPUContextVK::OnWrapAHardwareBuffer: AHardwareBuffer API not "
        "available (requires API 26+)");
    return {};
  }

  AHardwareBuffer_Desc ahb_desc = {};
  state_->GetAHardwareBufferDescribeFn()(ahb, &ahb_desc);

  if (width == 0) {
    width = ahb_desc.width;
  }
  if (height == 0) {
    height = ahb_desc.height;
  }

  // Step 2: Map AHB format to VkFormat
  VkFormat vk_format = VK_FORMAT_UNDEFINED;
  switch (ahb_desc.format) {
    case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
      vk_format = VK_FORMAT_R8G8B8A8_UNORM;
      break;
    case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
      vk_format = VK_FORMAT_R8G8B8A8_UNORM;
      break;
    case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
      vk_format = VK_FORMAT_R8G8B8_UNORM;
      break;
    case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
      vk_format = VK_FORMAT_R5G6B5_UNORM_PACK16;
      break;
    default:
      LOGE("GPUContextVK::OnWrapAHardwareBuffer: unsupported AHB format {}",
           static_cast<int32_t>(ahb_desc.format));
      return {};
  }

  GPUTextureFormat gpu_format = ToGPUTextureFormat(vk_format);
  if (gpu_format == GPUTextureFormat::kInvalid) {
    LOGE("GPUContextVK::OnWrapAHardwareBuffer: unsupported VkFormat {}",
         static_cast<int32_t>(vk_format));
    return {};
  }

  // Step 3: Query Vulkan memory properties from AHB
  VkAndroidHardwareBufferPropertiesANDROID ahb_props = {};
  ahb_props.sType =
      VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
  VkResult result =
      fns.vkGetAndroidHardwareBufferPropertiesANDROID(device, ahb, &ahb_props);
  if (result != VK_SUCCESS) {
    LOGE(
        "GPUContextVK::OnWrapAHardwareBuffer: "
        "vkGetAndroidHardwareBufferPropertiesANDROID failed: {}",
        static_cast<int32_t>(result));
    return {};
  }

  // Per VK_ANDROID_external_memory_android_hardware_buffer, the requested
  // VkImage usage must be derivable from the AHardwareBuffer's usage flags.
  // Blindly requesting an unsupported bit (e.g. always asking for
  // TRANSFER_DST) makes vkCreateImage fail on some devices. skity only samples
  // the imported buffer (its contents are produced by the external producer),
  // so SAMPLED is all we need.
  if ((ahb_desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) == 0) {
    LOGE(
        "GPUContextVK::OnWrapAHardwareBuffer: AHardwareBuffer is missing "
        "AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE and cannot be sampled");
    return {};
  }
  const VkImageUsageFlags image_usage = VK_IMAGE_USAGE_SAMPLED_BIT;

  // Step 4: Create VkImage with external memory
  VkExternalMemoryImageCreateInfo external_mem_info = {};
  external_mem_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  external_mem_info.handleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.pNext = &external_mem_info;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = vk_format;
  image_info.extent = {width, height, 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = image_usage;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImage image = VK_NULL_HANDLE;
  result = fns.vkCreateImage(device, &image_info, nullptr, &image);
  if (result != VK_SUCCESS) {
    LOGE("GPUContextVK::OnWrapAHardwareBuffer: vkCreateImage failed: {}",
         static_cast<int32_t>(result));
    return {};
  }

  // Step 5: Select memory type (prefer DEVICE_LOCAL)
  VkPhysicalDeviceMemoryProperties mem_props = {};
  state_->InstanceFns().vkGetPhysicalDeviceMemoryProperties(
      state_->GetPhysicalDevice(), &mem_props);
  uint32_t memory_type_index = UINT32_MAX;
  for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
    if ((ahb_props.memoryTypeBits & (1u << i)) != 0) {
      memory_type_index = i;
      if ((mem_props.memoryTypes[i].propertyFlags &
           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
        break;
      }
    }
  }
  if (memory_type_index == UINT32_MAX) {
    LOGE(
        "GPUContextVK::OnWrapAHardwareBuffer: "
        "no suitable memory type found");
    fns.vkDestroyImage(device, image, nullptr);
    return {};
  }

  // Step 6: Import AHB memory
  VkImportAndroidHardwareBufferInfoANDROID import_ahb_info = {};
  import_ahb_info.sType =
      VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
  import_ahb_info.buffer = ahb;

  VkMemoryDedicatedAllocateInfo dedicated_info = {};
  dedicated_info.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  dedicated_info.pNext = &import_ahb_info;
  dedicated_info.image = image;

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = &dedicated_info;
  alloc_info.allocationSize = ahb_props.allocationSize;
  alloc_info.memoryTypeIndex = memory_type_index;

  VkDeviceMemory memory = VK_NULL_HANDLE;
  result = fns.vkAllocateMemory(device, &alloc_info, nullptr, &memory);
  if (result != VK_SUCCESS) {
    LOGE("GPUContextVK::OnWrapAHardwareBuffer: vkAllocateMemory failed: {}",
         static_cast<int32_t>(result));
    fns.vkDestroyImage(device, image, nullptr);
    return {};
  }

  // Step 7: Bind image memory
  result = fns.vkBindImageMemory(device, image, memory, 0);
  if (result != VK_SUCCESS) {
    LOGE("GPUContextVK::OnWrapAHardwareBuffer: vkBindImageMemory failed: {}",
         static_cast<int32_t>(result));
    fns.vkFreeMemory(device, memory, nullptr);
    fns.vkDestroyImage(device, image, nullptr);
    return {};
  }

  // Step 8: Create ImageView
  VkImageViewCreateInfo view_info = {};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = vk_format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  VkImageView image_view = VK_NULL_HANDLE;
  result = fns.vkCreateImageView(device, &view_info, nullptr, &image_view);
  if (result != VK_SUCCESS) {
    LOGE("GPUContextVK::OnWrapAHardwareBuffer: vkCreateImageView failed: {}",
         static_cast<int32_t>(result));
    fns.vkFreeMemory(device, memory, nullptr);
    fns.vkDestroyImage(device, image, nullptr);
    return {};
  }

  // Step 9: Wrap into GPUExternalTextureAHB
  GPUTextureDescriptor desc = {};
  desc.width = width;
  desc.height = height;
  desc.mip_level_count = 1;
  desc.sample_count = 1;
  desc.format = gpu_format;
  desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
  desc.storage_mode = GPUTextureStorageMode::kPrivate;

  auto texture = GPUExternalTextureAHB::Make(
      state_, desc, image, memory, image_view, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vk_format);
  if (texture == nullptr) {
    LOGE(
        "GPUContextVK::OnWrapAHardwareBuffer: GPUExternalTextureAHB::Make "
        "failed");
    fns.vkDestroyImageView(device, image_view, nullptr);
    fns.vkFreeMemory(device, memory, nullptr);
    fns.vkDestroyImage(device, image, nullptr);
    return {};
  }

  texture->SetRelease(callback, user_data);
  return texture;
}
#endif  // defined(SKITY_ANDROID)

}  // namespace skity
