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

#ifndef SKITY_EXAMPLE_VK_ENABLE_VALIDATION
#if defined(NDEBUG)
#define SKITY_EXAMPLE_VK_ENABLE_VALIDATION 0
#else
#define SKITY_EXAMPLE_VK_ENABLE_VALIDATION 1
#endif
#endif

constexpr char kPortabilitySubsetExtensionName[] = "VK_KHR_portability_subset";
constexpr char kValidationLayerName[] = "VK_LAYER_KHRONOS_validation";
constexpr uint32_t kRequiredVulkanApiVersion = VK_API_VERSION_1_1;

bool ContainsLayer(const std::vector<VkLayerProperties>& layers,
                   const char* name) {
  return std::any_of(layers.begin(), layers.end(), [name](const auto& layer) {
    return std::string(layer.layerName) == name;
  });
}

bool ContainsExtension(const std::vector<VkExtensionProperties>& extensions,
                       const char* name) {
  return std::any_of(extensions.begin(), extensions.end(),
                     [name](const auto& extension) {
                       return std::string(extension.extensionName) == name;
                     });
}

void AddUniqueExtension(std::vector<const char*>* extensions,
                        const char* name) {
  if (extensions == nullptr || name == nullptr) {
    return;
  }

  if (std::find(extensions->begin(), extensions->end(), name) !=
      extensions->end()) {
    return;
  }

  extensions->push_back(name);
}

void AddUniqueLayer(std::vector<const char*>* layers, const char* name) {
  if (layers == nullptr || name == nullptr) {
    return;
  }

  if (std::find(layers->begin(), layers->end(), name) != layers->end()) {
    return;
  }

  layers->push_back(name);
}

VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  (void)message_severity;
  (void)message_types;
  (void)user_data;

  const char* message =
      callback_data != nullptr && callback_data->pMessage != nullptr
          ? callback_data->pMessage
          : "Unknown Vulkan validation message";
  std::cerr << "[Vulkan Validation] " << message << std::endl;
  return VK_FALSE;
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

  extent.width = std::clamp(extent.width, caps.minImageExtent.width,
                            caps.maxImageExtent.width);
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
  if (swapchain_ == VK_NULL_HANDLE || frame_sync_objects_.empty() ||
      swapchain_image_states_.size() != swapchain_images_.size()) {
    return nullptr;
  }

  FrameSyncObjects& frame_sync = frame_sync_objects_[current_frame_slot_];
  if (frame_sync.image_available_semaphore == VK_NULL_HANDLE ||
      frame_sync.in_flight_fence == VK_NULL_HANDLE) {
    return nullptr;
  }

  if (vkWaitForFences(device_, 1, &frame_sync.in_flight_fence, VK_TRUE,
                      UINT64_MAX) != VK_SUCCESS) {
    return nullptr;
  }

  VkResult result = vkAcquireNextImageKHR(
      device_, swapchain_, UINT64_MAX, frame_sync.image_available_semaphore,
      VK_NULL_HANDLE, &current_image_index_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
    return nullptr;
  }

  if (result != VK_SUCCESS) {
    return nullptr;
  }

  SwapchainImageState& image_state =
      swapchain_image_states_[current_image_index_];
  if (image_state.render_finished_semaphore == VK_NULL_HANDLE) {
    return nullptr;
  }

  if (image_state.in_flight_fence != VK_NULL_HANDLE &&
      vkWaitForFences(device_, 1, &image_state.in_flight_fence, VK_TRUE,
                      UINT64_MAX) != VK_SUCCESS) {
    return nullptr;
  }

  image_state.retired_surface.reset();
  image_state.in_flight_fence = VK_NULL_HANDLE;

  if (vkResetFences(device_, 1, &frame_sync.in_flight_fence) != VK_SUCCESS) {
    return nullptr;
  }

  surface_sync_info_.wait_semaphore = frame_sync.image_available_semaphore;
  surface_sync_info_.wait_dst_stage_mask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  surface_sync_info_.signal_semaphore = image_state.render_finished_semaphore;
  surface_sync_info_.signal_fence = frame_sync.in_flight_fence;

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

  const uint32_t image_index = current_image_index_;
  if (image_index >= swapchain_image_states_.size()) {
    canvas_ = nullptr;
    surface_.reset();
    return;
  }

  canvas_->Flush();
  surface_->Flush();
  canvas_ = nullptr;
  swapchain_image_states_[image_index].retired_surface = std::move(surface_);
  swapchain_image_states_[image_index].in_flight_fence =
      frame_sync_objects_[current_frame_slot_].in_flight_fence;

  VkSwapchainKHR swapchains[] = {swapchain_};
  VkSemaphore wait_semaphores[] = {
      swapchain_image_states_[image_index].render_finished_semaphore};
  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = wait_semaphores;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.pImageIndices = &image_index;

  VkResult result = vkQueuePresentKHR(graphics_queue_, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    glfwSetWindowShouldClose(GetNativeWindow(), GLFW_TRUE);
  }

  current_frame_slot_ =
      (current_frame_slot_ + 1u) %
      static_cast<uint32_t>(std::max<size_t>(frame_sync_objects_.size(), 1u));
}

void WindowVK::OnTerminate() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
  }

  canvas_ = nullptr;
  surface_.reset();

  for (auto& image_state : swapchain_image_states_) {
    image_state.retired_surface.reset();
    image_state.in_flight_fence = VK_NULL_HANDLE;
  }

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

  if (debug_messenger_ != VK_NULL_HANDLE) {
    vkDestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
    debug_messenger_ = VK_NULL_HANDLE;
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

  uint32_t supported_instance_version = VK_API_VERSION_1_0;
  if (vkEnumerateInstanceVersion != nullptr &&
      vkEnumerateInstanceVersion(&supported_instance_version) != VK_SUCCESS) {
    std::cerr << "Failed to query Vulkan instance version." << std::endl;
    return false;
  }

  if (VK_API_VERSION_MAJOR(supported_instance_version) <
          VK_API_VERSION_MAJOR(kRequiredVulkanApiVersion) ||
      (VK_API_VERSION_MAJOR(supported_instance_version) ==
           VK_API_VERSION_MAJOR(kRequiredVulkanApiVersion) &&
       VK_API_VERSION_MINOR(supported_instance_version) <
           VK_API_VERSION_MINOR(kRequiredVulkanApiVersion))) {
    std::cerr << "Vulkan 1.1 instance support is required, but only Vulkan "
              << VK_API_VERSION_MAJOR(supported_instance_version) << "."
              << VK_API_VERSION_MINOR(supported_instance_version)
              << " is available." << std::endl;
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

  enabled_instance_extensions_.assign(
      required_extensions, required_extensions + required_extension_count);
  enabled_instance_layers_.clear();

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

  uint32_t layer_count = 0;
  if (vkEnumerateInstanceLayerProperties(&layer_count, nullptr) != VK_SUCCESS) {
    return false;
  }

  std::vector<VkLayerProperties> layers(layer_count);
  if (layer_count > 0 && vkEnumerateInstanceLayerProperties(
                             &layer_count, layers.data()) != VK_SUCCESS) {
    return false;
  }

  VkInstanceCreateFlags create_flags = 0;
  if (ContainsExtension(extensions,
                        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    AddUniqueExtension(&enabled_instance_extensions_,
                       VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    create_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }

#if SKITY_EXAMPLE_VK_ENABLE_VALIDATION
  if (ContainsExtension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    AddUniqueExtension(&enabled_instance_extensions_,
                       VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  } else {
    std::cerr << "Vulkan debug extension " << VK_EXT_DEBUG_UTILS_EXTENSION_NAME
              << " is unavailable, skipping debug messenger." << std::endl;
  }

  if (ContainsLayer(layers, kValidationLayerName)) {
    AddUniqueLayer(&enabled_instance_layers_, kValidationLayerName);
  } else {
    std::cerr << "Vulkan validation layer " << kValidationLayerName
              << " is unavailable, continuing without validation." << std::endl;
  }
#endif

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Skity Example";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "Skity";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = kRequiredVulkanApiVersion;

  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.flags = create_flags;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(enabled_instance_extensions_.size());
  create_info.ppEnabledExtensionNames = enabled_instance_extensions_.data();
  create_info.enabledLayerCount =
      static_cast<uint32_t>(enabled_instance_layers_.size());
  create_info.ppEnabledLayerNames = enabled_instance_layers_.data();

  if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS ||
      instance_ == VK_NULL_HANDLE) {
    std::cerr << "Failed to create Vulkan instance." << std::endl;
    return false;
  }

  volkLoadInstance(instance_);

#if SKITY_EXAMPLE_VK_ENABLE_VALIDATION
  if (std::find(enabled_instance_extensions_.begin(),
                enabled_instance_extensions_.end(),
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME) !=
      enabled_instance_extensions_.end()) {
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
    debug_create_info.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_create_info.pfnUserCallback = DebugUtilsMessengerCallback;

    if (vkCreateDebugUtilsMessengerEXT(instance_, &debug_create_info, nullptr,
                                       &debug_messenger_) != VK_SUCCESS) {
      std::cerr << "Failed to create Vulkan debug utils messenger."
                << std::endl;
      debug_messenger_ = VK_NULL_HANDLE;
    }
  }
#endif

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
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    if (VK_API_VERSION_MAJOR(properties.apiVersion) <
            VK_API_VERSION_MAJOR(kRequiredVulkanApiVersion) ||
        (VK_API_VERSION_MAJOR(properties.apiVersion) ==
             VK_API_VERSION_MAJOR(kRequiredVulkanApiVersion) &&
         VK_API_VERSION_MINOR(properties.apiVersion) <
             VK_API_VERSION_MINOR(kRequiredVulkanApiVersion))) {
      continue;
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_count, nullptr);
    if (queue_family_count == 0) {
      continue;
    }

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, queue_families.data());

    for (uint32_t index = 0; index < queue_family_count; ++index) {
      VkBool32 present_supported = VK_FALSE;
      if (vkGetPhysicalDeviceSurfaceSupportKHR(
              physical_device, index, surface_khr_, &present_supported) !=
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
  if (vkEnumerateDeviceExtensionProperties(
          physical_device_, nullptr, &extension_count, nullptr) != VK_SUCCESS) {
    return false;
  }

  std::vector<VkExtensionProperties> extensions(extension_count);
  if (extension_count > 0 && vkEnumerateDeviceExtensionProperties(
                                 physical_device_, nullptr, &extension_count,
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
  device_create_info.ppEnabledExtensionNames =
      enabled_device_extensions_.data();

  if (vkCreateDevice(physical_device_, &device_create_info, nullptr,
                     &device_) != VK_SUCCESS ||
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
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_khr_,
                                                &present_mode_count,
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
  frame_sync_objects_.clear();
  swapchain_image_states_.clear();

  if (swapchain_images_.empty()) {
    return false;
  }

  VkSemaphoreCreateInfo semaphore_create_info = {};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_create_info = {};
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  frame_sync_objects_.resize(swapchain_images_.size());
  for (auto& frame_sync : frame_sync_objects_) {
    if (vkCreateSemaphore(device_, &semaphore_create_info, nullptr,
                          &frame_sync.image_available_semaphore) !=
            VK_SUCCESS ||
        vkCreateFence(device_, &fence_create_info, nullptr,
                      &frame_sync.in_flight_fence) != VK_SUCCESS) {
      DestroyFrameSyncObjects();
      return false;
    }
  }

  swapchain_image_states_.resize(swapchain_images_.size());
  for (auto& image_state : swapchain_image_states_) {
    if (vkCreateSemaphore(device_, &semaphore_create_info, nullptr,
                          &image_state.render_finished_semaphore) !=
        VK_SUCCESS) {
      DestroyFrameSyncObjects();
      return false;
    }
  }

  current_frame_slot_ = 0;
  current_image_index_ = 0;
  return true;
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
  for (auto& image_state : swapchain_image_states_) {
    image_state.retired_surface.reset();
    image_state.in_flight_fence = VK_NULL_HANDLE;
    if (image_state.render_finished_semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device_, image_state.render_finished_semaphore,
                         nullptr);
      image_state.render_finished_semaphore = VK_NULL_HANDLE;
    }
  }
  swapchain_image_states_.clear();

  for (auto& frame_sync : frame_sync_objects_) {
    if (frame_sync.in_flight_fence != VK_NULL_HANDLE) {
      vkDestroyFence(device_, frame_sync.in_flight_fence, nullptr);
      frame_sync.in_flight_fence = VK_NULL_HANDLE;
    }
    if (frame_sync.image_available_semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device_, frame_sync.image_available_semaphore,
                         nullptr);
      frame_sync.image_available_semaphore = VK_NULL_HANDLE;
    }
  }
  frame_sync_objects_.clear();
}

}  // namespace example
}  // namespace skity
