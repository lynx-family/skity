// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/vk/window_vk.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace skity {
namespace example {

namespace {

constexpr char kPortabilitySubsetExtensionName[] = "VK_KHR_portability_subset";

bool ContainsExtension(const std::vector<VkExtensionProperties>& extensions,
                       const char* name) {
  return std::any_of(extensions.begin(), extensions.end(),
                     [name](const auto& extension) {
                       return std::string(extension.extensionName) == name;
                     });
}

void AddUniqueExtension(std::vector<const char*>* extensions, const char* name) {
  if (extensions == nullptr || name == nullptr) {
    return;
  }

  if (std::find(extensions->begin(), extensions->end(), name) !=
      extensions->end()) {
    return;
  }

  extensions->push_back(name);
}

VkSurfaceFormatKHR ChooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) {
  for (const auto& format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }

  return formats.empty() ? VkSurfaceFormatKHR{} : formats.front();
}

VkPresentModeKHR ChoosePresentMode(
    const std::vector<VkPresentModeKHR>& present_modes) {
  for (auto present_mode : present_modes) {
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return present_mode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSwapchainExtent(GLFWwindow* window,
                                 const VkSurfaceCapabilitiesKHR& caps) {
  if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return caps.currentExtent;
  }

  int framebuffer_width = 0;
  int framebuffer_height = 0;
  glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

  VkExtent2D extent = {
      static_cast<uint32_t>(std::max(framebuffer_width, 1)),
      static_cast<uint32_t>(std::max(framebuffer_height, 1)),
  };

  extent.width =
      std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
  extent.height = std::clamp(extent.height, caps.minImageExtent.height,
                             caps.maxImageExtent.height);
  return extent;
}

}  // namespace

bool WindowVK::OnInit() {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  return true;
}

GLFWwindow* WindowVK::CreateWindowHandler() {
  return glfwCreateWindow(GetWidth(), GetHeight(), GetTitle().c_str(), nullptr,
                          nullptr);
}

std::unique_ptr<skity::GPUContext> WindowVK::CreateGPUContext() {
  if (!CreateInstance() || !CreateSurface() ||
      !PickPhysicalDeviceAndQueueFamily() || !CreateLogicalDevice()) {
    return nullptr;
  }

  skity::GPUContextInfoVK info = {};
  info.instance = instance_;
  info.get_instance_proc_addr = vkGetInstanceProcAddr;
  info.enabled_instance_extensions = enabled_instance_extensions_.data();
  info.enabled_instance_extension_count =
      static_cast<uint32_t>(enabled_instance_extensions_.size());
  info.enabled_instance_extensions_known = true;
  info.physical_device = physical_device_;
  info.logical_device = device_;
  info.get_device_proc_addr = vkGetDeviceProcAddr;
  info.enabled_device_extensions = enabled_device_extensions_.data();
  info.enabled_device_extension_count =
      static_cast<uint32_t>(enabled_device_extensions_.size());
  info.enabled_device_extensions_known = true;
  info.graphics_queue = graphics_queue_;
  info.graphics_queue_family_index =
      static_cast<int32_t>(graphics_queue_family_index_);

  return skity::CreateGPUContextVK(&info);
}

void WindowVK::OnShow() {
  if (!CreateSwapchain() || !CreateSwapchainImageViews() ||
      !CreateFrameSyncObjects()) {
    std::cerr << "Failed to prepare Vulkan swapchain resources." << std::endl;
    glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
  }
}

skity::Canvas* WindowVK::AquireCanvas() {
  if (swapchain_ == VK_NULL_HANDLE || image_available_semaphore_ == VK_NULL_HANDLE ||
      render_finished_semaphore_ == VK_NULL_HANDLE ||
      in_flight_fence_ == VK_NULL_HANDLE) {
    return nullptr;
  }

  if (vkWaitForFences(device_, 1, &in_flight_fence_, VK_TRUE, UINT64_MAX) !=
      VK_SUCCESS) {
    return nullptr;
  }

  if (vkResetFences(device_, 1, &in_flight_fence_) != VK_SUCCESS) {
    return nullptr;
  }

  VkResult result =
      vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                            image_available_semaphore_, VK_NULL_HANDLE,
                            &current_image_index_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
    return nullptr;
  }

  if (result != VK_SUCCESS) {
    return nullptr;
  }

  surface_sync_info_.wait_semaphore = image_available_semaphore_;
  surface_sync_info_.wait_dst_stage_mask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  surface_sync_info_.signal_semaphore = render_finished_semaphore_;
  surface_sync_info_.signal_fence = in_flight_fence_;

  skity::GPUSurfaceDescriptorVK desc = {};
  desc.backend = skity::GPUBackendType::kVulkan;
  desc.width = swapchain_extent_.width;
  desc.height = swapchain_extent_.height;
  desc.content_scale = 1.f;
  desc.sample_count = 1;
  desc.surface_type = skity::VKSurfaceType::kSwapchainImage;
  desc.image = swapchain_images_[current_image_index_];
  desc.image_view = swapchain_image_views_[current_image_index_];
  desc.format = swapchain_format_;
  desc.pre_transform = swapchain_transform_;
  desc.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  desc.final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  desc.sync_info = &surface_sync_info_;

  surface_ = GetGPUContext()->CreateSurface(&desc);
  if (surface_ == nullptr) {
    return nullptr;
  }

  canvas_ = surface_->LockCanvas();
  return canvas_;
}

void WindowVK::OnPresent() {
  if (canvas_ == nullptr || surface_ == nullptr) {
    return;
  }

  canvas_->Flush();
  surface_->Flush();
  canvas_ = nullptr;
  surface_.reset();

  VkSwapchainKHR swapchains[] = {swapchain_};
  VkSemaphore wait_semaphores[] = {render_finished_semaphore_};
  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = wait_semaphores;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.pImageIndices = &current_image_index_;

  VkResult result = vkQueuePresentKHR(graphics_queue_, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
  }
}

void WindowVK::OnTerminate() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
  }

  surface_.reset();
  canvas_ = nullptr;
  ResetGPUContext();

  DestroyFrameSyncObjects();
  DestroySwapchain();

  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }

  if (surface_khr_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_khr_, nullptr);
    surface_khr_ = VK_NULL_HANDLE;
  }

  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
  }
}

bool WindowVK::CreateInstance() {
  if (volkInitialize() != VK_SUCCESS) {
    std::cerr << "Failed to initialize volk." << std::endl;
    return false;
  }

  uint32_t required_extension_count = 0;
  const char** required_extensions =
      glfwGetRequiredInstanceExtensions(&required_extension_count);
  if (required_extensions == nullptr || required_extension_count == 0) {
    std::cerr << "Failed to query GLFW Vulkan instance extensions."
              << std::endl;
    return false;
  }

  enabled_instance_extensions_.assign(required_extensions,
                                      required_extensions +
                                          required_extension_count);

  uint32_t extension_count = 0;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                             nullptr) != VK_SUCCESS) {
    return false;
  }

  std::vector<VkExtensionProperties> extensions(extension_count);
  if (extension_count > 0 &&
      vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                             extensions.data()) != VK_SUCCESS) {
    return false;
  }

  VkInstanceCreateFlags create_flags = 0;
  if (ContainsExtension(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    AddUniqueExtension(&enabled_instance_extensions_,
                       VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    create_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Skity Example";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "Skity";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.flags = create_flags;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(enabled_instance_extensions_.size());
  create_info.ppEnabledExtensionNames = enabled_instance_extensions_.data();

  if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS ||
      instance_ == VK_NULL_HANDLE) {
    std::cerr << "Failed to create Vulkan instance." << std::endl;
    return false;
  }

  volkLoadInstance(instance_);
  return true;
}

bool WindowVK::CreateSurface() {
  if (glfwCreateWindowSurface(instance_, GetNativeWindow(), nullptr,
                              &surface_khr_) != VK_SUCCESS ||
      surface_khr_ == VK_NULL_HANDLE) {
    std::cerr << "Failed to create Vulkan window surface." << std::endl;
    return false;
  }

  return true;
}

bool WindowVK::PickPhysicalDeviceAndQueueFamily() {
  uint32_t physical_device_count = 0;
  if (vkEnumeratePhysicalDevices(instance_, &physical_device_count, nullptr) !=
          VK_SUCCESS ||
      physical_device_count == 0) {
    return false;
  }

  std::vector<VkPhysicalDevice> physical_devices(physical_device_count,
                                                 VK_NULL_HANDLE);
  if (vkEnumeratePhysicalDevices(instance_, &physical_device_count,
                                 physical_devices.data()) != VK_SUCCESS) {
    return false;
  }

  for (auto physical_device : physical_devices) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_count, nullptr);
    if (queue_family_count == 0) {
      continue;
    }

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_count,
                                             queue_families.data());

    for (uint32_t index = 0; index < queue_family_count; ++index) {
      VkBool32 present_supported = VK_FALSE;
      if (vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index,
                                               surface_khr_,
                                               &present_supported) !=
          VK_SUCCESS) {
        continue;
      }

      const bool graphics_supported =
          (queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
      if (!graphics_supported || present_supported != VK_TRUE) {
        continue;
      }

      physical_device_ = physical_device;
      graphics_queue_family_index_ = index;
      return true;
    }
  }

  return false;
}

bool WindowVK::CreateLogicalDevice() {
  uint32_t extension_count = 0;
  if (vkEnumerateDeviceExtensionProperties(physical_device_, nullptr,
                                           &extension_count,
                                           nullptr) != VK_SUCCESS) {
    return false;
  }

  std::vector<VkExtensionProperties> extensions(extension_count);
  if (extension_count > 0 &&
      vkEnumerateDeviceExtensionProperties(physical_device_, nullptr,
                                           &extension_count,
                                           extensions.data()) != VK_SUCCESS) {
    return false;
  }

  enabled_device_extensions_.clear();
  AddUniqueExtension(&enabled_device_extensions_,
                     VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  if (ContainsExtension(extensions, kPortabilitySubsetExtensionName)) {
    AddUniqueExtension(&enabled_device_extensions_,
                       kPortabilitySubsetExtensionName);
  }

  float queue_priority = 1.f;
  VkDeviceQueueCreateInfo queue_create_info = {};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = graphics_queue_family_index_;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  VkDeviceCreateInfo device_create_info = {};
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledExtensionCount =
      static_cast<uint32_t>(enabled_device_extensions_.size());
  device_create_info.ppEnabledExtensionNames = enabled_device_extensions_.data();

  if (vkCreateDevice(physical_device_, &device_create_info, nullptr, &device_) !=
          VK_SUCCESS ||
      device_ == VK_NULL_HANDLE) {
    std::cerr << "Failed to create Vulkan logical device." << std::endl;
    return false;
  }

  volkLoadDevice(device_);
  vkGetDeviceQueue(device_, graphics_queue_family_index_, 0, &graphics_queue_);
  return graphics_queue_ != VK_NULL_HANDLE;
}

bool WindowVK::CreateSwapchain() {
  VkSurfaceCapabilitiesKHR caps = {};
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_khr_,
                                                &caps) != VK_SUCCESS) {
    return false;
  }

  uint32_t format_count = 0;
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_khr_,
                                           &format_count,
                                           nullptr) != VK_SUCCESS ||
      format_count == 0) {
    return false;
  }

  std::vector<VkSurfaceFormatKHR> formats(format_count);
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_khr_,
                                           &format_count,
                                           formats.data()) != VK_SUCCESS) {
    return false;
  }

  uint32_t present_mode_count = 0;
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(
          physical_device_, surface_khr_, &present_mode_count,
          nullptr) != VK_SUCCESS ||
      present_mode_count == 0) {
    return false;
  }

  std::vector<VkPresentModeKHR> present_modes(present_mode_count);
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(
          physical_device_, surface_khr_, &present_mode_count,
          present_modes.data()) != VK_SUCCESS) {
    return false;
  }

  VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(formats);
  VkPresentModeKHR present_mode = ChoosePresentMode(present_modes);
  VkExtent2D extent = ChooseSwapchainExtent(GetNativeWindow(), caps);

  uint32_t image_count = caps.minImageCount + 1;
  if (caps.maxImageCount > 0) {
    image_count = std::min(image_count, caps.maxImageCount);
  }

  VkSwapchainCreateInfoKHR create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface_khr_;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.preTransform = caps.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;

  if (vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_) !=
          VK_SUCCESS ||
      swapchain_ == VK_NULL_HANDLE) {
    return false;
  }

  swapchain_format_ = surface_format.format;
  swapchain_extent_ = extent;
  swapchain_transform_ =
      static_cast<VkSurfaceTransformFlagBitsKHR>(caps.currentTransform);

  uint32_t swapchain_image_count = 0;
  if (vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count,
                              nullptr) != VK_SUCCESS ||
      swapchain_image_count == 0) {
    return false;
  }

  swapchain_images_.resize(swapchain_image_count, VK_NULL_HANDLE);
  if (vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count,
                              swapchain_images_.data()) != VK_SUCCESS) {
    return false;
  }

  return true;
}

bool WindowVK::CreateSwapchainImageViews() {
  swapchain_image_views_.clear();
  swapchain_image_views_.reserve(swapchain_images_.size());

  for (auto image : swapchain_images_) {
    VkImageViewCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = image;
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = swapchain_format_;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    VkImageView image_view = VK_NULL_HANDLE;
    if (vkCreateImageView(device_, &create_info, nullptr, &image_view) !=
            VK_SUCCESS ||
        image_view == VK_NULL_HANDLE) {
      return false;
    }

    swapchain_image_views_.push_back(image_view);
  }

  return true;
}

bool WindowVK::CreateFrameSyncObjects() {
  VkSemaphoreCreateInfo semaphore_create_info = {};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  if (vkCreateSemaphore(device_, &semaphore_create_info, nullptr,
                        &image_available_semaphore_) != VK_SUCCESS ||
      vkCreateSemaphore(device_, &semaphore_create_info, nullptr,
                        &render_finished_semaphore_) != VK_SUCCESS) {
    return false;
  }

  VkFenceCreateInfo fence_create_info = {};
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  return vkCreateFence(device_, &fence_create_info, nullptr,
                       &in_flight_fence_) == VK_SUCCESS;
}

void WindowVK::DestroySwapchain() {
  for (auto image_view : swapchain_image_views_) {
    if (image_view != VK_NULL_HANDLE) {
      vkDestroyImageView(device_, image_view, nullptr);
    }
  }
  swapchain_image_views_.clear();
  swapchain_images_.clear();

  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
}

void WindowVK::DestroyFrameSyncObjects() {
  if (in_flight_fence_ != VK_NULL_HANDLE) {
    vkDestroyFence(device_, in_flight_fence_, nullptr);
    in_flight_fence_ = VK_NULL_HANDLE;
  }

  if (render_finished_semaphore_ != VK_NULL_HANDLE) {
    vkDestroySemaphore(device_, render_finished_semaphore_, nullptr);
    render_finished_semaphore_ = VK_NULL_HANDLE;
  }

  if (image_available_semaphore_ != VK_NULL_HANDLE) {
    vkDestroySemaphore(device_, image_available_semaphore_, nullptr);
    image_available_semaphore_ = VK_NULL_HANDLE;
  }
}

}  // namespace example
}  // namespace skity
