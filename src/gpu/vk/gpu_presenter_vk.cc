// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_presenter_vk.hpp"

#include <algorithm>
#include <cmath>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

VkImageView CreateSwapchainImageView(const VulkanContextState& state,
                                     VkImage image, VkFormat format) {
  VkImageViewCreateInfo view_info = {};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  VkImageView image_view = VK_NULL_HANDLE;
  const VkResult result = state.DeviceFns().vkCreateImageView(
      state.GetLogicalDevice(), &view_info, nullptr, &image_view);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create swapchain image view: {}",
         static_cast<int32_t>(result));
    return VK_NULL_HANDLE;
  }

  return image_view;
}

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
    default:
      return GPUTextureFormat::kInvalid;
  }
}

}  // namespace

GPUPresenterVK::GPUPresenterVK(GPUContextImpl* context,
                               std::shared_ptr<const VulkanContextState> state,
                               const GPUPresenterDescriptorVK& desc)
    : context_(context), state_(std::move(state)), desc_(desc) {}

GPUPresenterVK::~GPUPresenterVK() { Reset(); }

bool GPUPresenterVK::Init() {
  if (context_ == nullptr || state_ == nullptr ||
      desc_.surface == VK_NULL_HANDLE || desc_.width == 0 ||
      desc_.height == 0) {
    LOGE("Failed to initialize Vulkan presenter: invalid descriptor");
    return false;
  }

  present_queue_ = desc_.present_queue != VK_NULL_HANDLE
                       ? desc_.present_queue
                       : state_->GetGraphicsQueue();
  present_queue_family_index_ = desc_.present_queue_family_index >= 0
                                    ? desc_.present_queue_family_index
                                    : state_->GetGraphicsQueueFamilyIndex();

  if (present_queue_ == VK_NULL_HANDLE || present_queue_family_index_ < 0) {
    LOGE("Failed to initialize Vulkan presenter: invalid present queue");
    return false;
  }

  if (!LoadPresenterFns()) {
    return false;
  }

  VkBool32 supported = VK_FALSE;
  const VkResult support_result = fns_.vkGetPhysicalDeviceSurfaceSupportKHR(
      state_->GetPhysicalDevice(),
      static_cast<uint32_t>(present_queue_family_index_), desc_.surface,
      &supported);
  if (support_result != VK_SUCCESS || supported != VK_TRUE) {
    LOGE("Failed to initialize Vulkan presenter: present queue unsupported");
    return false;
  }

  if (!CreateSwapchain() || !CreateSwapchainImageViews() ||
      !CreateFrameSlots()) {
    Reset();
    return false;
  }

  return true;
}

GPUSurfaceAcquireResult GPUPresenterVK::AcquireNextSurface(
    const GPUSurfaceAcquireDescriptor& acquire_desc) {
  GPUSurfaceAcquireResult result = {};
  if (state_ == nullptr || swapchain_ == VK_NULL_HANDLE ||
      frame_slots_.empty() || has_outstanding_surface_) {
    return result;
  }

  const auto& device_fns = state_->DeviceFns();
  FrameSlot& frame_slot = frame_slots_[current_frame_];

  if (device_fns.vkWaitForFences(state_->GetLogicalDevice(), 1,
                                 &frame_slot.in_flight, VK_TRUE,
                                 UINT64_MAX) != VK_SUCCESS) {
    LOGE("Failed to wait for Vulkan presenter fence");
    return result;
  }

  if (fns_.vkResetFences(state_->GetLogicalDevice(), 1,
                         &frame_slot.in_flight) != VK_SUCCESS) {
    LOGE("Failed to reset Vulkan presenter fence");
    return result;
  }

  uint32_t image_index = 0;
  const VkResult acquire_result = fns_.vkAcquireNextImageKHR(
      state_->GetLogicalDevice(), swapchain_, UINT64_MAX,
      frame_slot.image_available, VK_NULL_HANDLE, &image_index);
  if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR ||
      acquire_result == VK_SUBOPTIMAL_KHR) {
    result.status = GPUPresenterStatus::kNeedRecreate;
    return result;
  }
  if (acquire_result != VK_SUCCESS) {
    LOGE("Failed to acquire swapchain image: {}",
         static_cast<int32_t>(acquire_result));
    return result;
  }

  if (image_index >= swapchain_images_.size() ||
      image_index >= swapchain_image_views_.size()) {
    LOGE("Failed to acquire swapchain image: invalid image index {}",
         image_index);
    return result;
  }

  VkFence& image_fence = image_in_flight_fences_[image_index];
  if (image_fence != VK_NULL_HANDLE && image_fence != frame_slot.in_flight) {
    if (device_fns.vkWaitForFences(state_->GetLogicalDevice(), 1, &image_fence,
                                   VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
      LOGE("Failed to wait for Vulkan image fence");
      return result;
    }
  }
  image_fence = frame_slot.in_flight;

  GPUSurfaceSyncInfoVK sync_info = {};
  sync_info.wait_semaphore = frame_slot.image_available;
  sync_info.wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  sync_info.signal_semaphore = frame_slot.render_finished;
  sync_info.signal_fence = frame_slot.in_flight;

  const uint32_t target_width = static_cast<uint32_t>(
      std::floor(static_cast<float>(desc_.width) / acquire_desc.content_scale));
  const uint32_t target_height = static_cast<uint32_t>(std::floor(
      static_cast<float>(desc_.height) / acquire_desc.content_scale));
  if (target_width == 0 || target_height == 0) {
    LOGE("Failed to acquire Vulkan surface: invalid target size");
    return result;
  }

  GPUSurfaceDescriptorVK surface_desc = {};
  surface_desc.backend = GPUBackendType::kVulkan;
  surface_desc.width = target_width;
  surface_desc.height = target_height;
  surface_desc.sample_count = acquire_desc.sample_count;
  surface_desc.content_scale = acquire_desc.content_scale;
  surface_desc.surface_type = VKSurfaceType::kSwapchainImage;
  surface_desc.image = swapchain_images_[image_index];
  surface_desc.image_view = swapchain_image_views_[image_index];
  surface_desc.format = swapchain_format_;
  surface_desc.pre_transform = swapchain_transform_;
  surface_desc.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  surface_desc.final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  surface_desc.owns_image = false;
  surface_desc.owns_image_view = false;
  surface_desc.sync_info = &sync_info;

  GPUTextureDescriptor texture_desc = {};
  texture_desc.width = desc_.width;
  texture_desc.height = desc_.height;
  texture_desc.mip_level_count = 1;
  texture_desc.sample_count = acquire_desc.sample_count;
  texture_desc.format = ToGPUTextureFormat(swapchain_format_);
  texture_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);
  texture_desc.storage_mode = GPUTextureStorageMode::kPrivate;
  if (texture_desc.format == GPUTextureFormat::kInvalid) {
    LOGE("Failed to acquire Vulkan surface: unsupported swapchain format");
    return result;
  }

  auto texture = GPUTextureVK::Wrap(
      state_, texture_desc, surface_desc.image, surface_desc.image_view,
      surface_desc.initial_layout, surface_desc.final_layout,
      surface_desc.format, false, false);
  if (texture == nullptr) {
    return result;
  }

  auto surface = std::make_unique<GPUSurfaceVK>(
      surface_desc, context_, std::move(texture), texture_desc.format,
      surface_desc.sync_info);
  GPUSurfaceVK::PresentInfo present_info = {};
  present_info.owner = this;
  present_info.image_index = image_index;
  surface->SetPresentInfo(present_info);
  has_outstanding_surface_ = true;
  result.status = GPUPresenterStatus::kSuccess;
  result.surface = std::move(surface);
  return result;
}

GPUPresenterStatus GPUPresenterVK::Present(
    std::unique_ptr<GPUSurface> surface) {
  if (!has_outstanding_surface_ || surface == nullptr ||
      surface->GetBackendType() != GPUBackendType::kVulkan) {
    return GPUPresenterStatus::kError;
  }

  auto* surface_vk = static_cast<GPUSurfaceVK*>(surface.get());
  const auto* present_info = surface_vk->GetPresentInfo();
  const auto* submit_info =
      static_cast<const GPUSubmitInfoVK*>(surface_vk->GetSubmitInfo());
  if (present_info == nullptr || present_info->owner != this ||
      submit_info == nullptr) {
    return GPUPresenterStatus::kError;
  }

  VkSemaphore wait_semaphore = submit_info->signal_semaphore;
  VkPresentInfoKHR present_info_vk = {};
  present_info_vk.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  if (wait_semaphore != VK_NULL_HANDLE) {
    present_info_vk.waitSemaphoreCount = 1;
    present_info_vk.pWaitSemaphores = &wait_semaphore;
  }
  present_info_vk.swapchainCount = 1;
  present_info_vk.pSwapchains = &swapchain_;
  present_info_vk.pImageIndices = &present_info->image_index;

  const VkResult result =
      fns_.vkQueuePresentKHR(present_queue_, &present_info_vk);
  has_outstanding_surface_ = false;
  current_frame_ = (current_frame_ + 1) % frame_slots_.size();
  if (result == VK_SUCCESS) {
    return GPUPresenterStatus::kSuccess;
  }
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    return GPUPresenterStatus::kNeedRecreate;
  }
  return GPUPresenterStatus::kError;
}

bool GPUPresenterVK::LoadPresenterFns() {
  if (state_ == nullptr || state_->GetInstanceProcAddr() == nullptr ||
      state_->GetDeviceProcAddr() == nullptr ||
      state_->GetInstance() == VK_NULL_HANDLE ||
      state_->GetLogicalDevice() == VK_NULL_HANDLE) {
    LOGE("Failed to load Vulkan presenter procedures: invalid state");
    return false;
  }

  auto load_instance = [this](const char* name) -> PFN_vkVoidFunction {
    return state_->GetInstanceProcAddr()(state_->GetInstance(), name);
  };
  auto load_device = [this](const char* name) -> PFN_vkVoidFunction {
    return state_->GetDeviceProcAddr()(state_->GetLogicalDevice(), name);
  };

  fns_.vkGetPhysicalDeviceSurfaceCapabilitiesKHR =
      reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
          load_instance("vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
  fns_.vkGetPhysicalDeviceSurfaceFormatsKHR =
      reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
          load_instance("vkGetPhysicalDeviceSurfaceFormatsKHR"));
  fns_.vkGetPhysicalDeviceSurfacePresentModesKHR =
      reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
          load_instance("vkGetPhysicalDeviceSurfacePresentModesKHR"));
  fns_.vkGetPhysicalDeviceSurfaceSupportKHR =
      reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
          load_instance("vkGetPhysicalDeviceSurfaceSupportKHR"));
  fns_.vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
      load_device("vkCreateSwapchainKHR"));
  fns_.vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
      load_device("vkDestroySwapchainKHR"));
  fns_.vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
      load_device("vkGetSwapchainImagesKHR"));
  fns_.vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
      load_device("vkAcquireNextImageKHR"));
  fns_.vkQueuePresentKHR =
      reinterpret_cast<PFN_vkQueuePresentKHR>(load_device("vkQueuePresentKHR"));
  fns_.vkCreateSemaphore =
      reinterpret_cast<PFN_vkCreateSemaphore>(load_device("vkCreateSemaphore"));
  fns_.vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(
      load_device("vkDestroySemaphore"));
  fns_.vkResetFences =
      reinterpret_cast<PFN_vkResetFences>(load_device("vkResetFences"));

  if (fns_.vkGetPhysicalDeviceSurfaceCapabilitiesKHR == nullptr ||
      fns_.vkGetPhysicalDeviceSurfaceFormatsKHR == nullptr ||
      fns_.vkGetPhysicalDeviceSurfacePresentModesKHR == nullptr ||
      fns_.vkGetPhysicalDeviceSurfaceSupportKHR == nullptr ||
      fns_.vkCreateSwapchainKHR == nullptr ||
      fns_.vkDestroySwapchainKHR == nullptr ||
      fns_.vkGetSwapchainImagesKHR == nullptr ||
      fns_.vkAcquireNextImageKHR == nullptr ||
      fns_.vkQueuePresentKHR == nullptr || fns_.vkCreateSemaphore == nullptr ||
      fns_.vkDestroySemaphore == nullptr || fns_.vkResetFences == nullptr) {
    LOGE("Failed to load Vulkan presenter procedures");
    return false;
  }

  return true;
}

bool GPUPresenterVK::CreateSwapchain() {
  VkSurfaceCapabilitiesKHR capabilities = {};
  if (fns_.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          state_->GetPhysicalDevice(), desc_.surface, &capabilities) !=
      VK_SUCCESS) {
    LOGE("Failed to query Vulkan surface capabilities");
    return false;
  }

  uint32_t format_count = 0;
  if (fns_.vkGetPhysicalDeviceSurfaceFormatsKHR(state_->GetPhysicalDevice(),
                                                desc_.surface, &format_count,
                                                nullptr) != VK_SUCCESS ||
      format_count == 0) {
    LOGE("Failed to query Vulkan surface formats");
    return false;
  }
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  if (fns_.vkGetPhysicalDeviceSurfaceFormatsKHR(state_->GetPhysicalDevice(),
                                                desc_.surface, &format_count,
                                                formats.data()) != VK_SUCCESS) {
    LOGE("Failed to load Vulkan surface formats");
    return false;
  }

  uint32_t present_mode_count = 0;
  if (fns_.vkGetPhysicalDeviceSurfacePresentModesKHR(
          state_->GetPhysicalDevice(), desc_.surface, &present_mode_count,
          nullptr) != VK_SUCCESS ||
      present_mode_count == 0) {
    LOGE("Failed to query Vulkan present modes");
    return false;
  }
  std::vector<VkPresentModeKHR> present_modes(present_mode_count);
  if (fns_.vkGetPhysicalDeviceSurfacePresentModesKHR(
          state_->GetPhysicalDevice(), desc_.surface, &present_mode_count,
          present_modes.data()) != VK_SUCCESS) {
    LOGE("Failed to load Vulkan present modes");
    return false;
  }

  VkSurfaceFormatKHR selected_format = formats.front();
  for (const auto& surface_format : formats) {
    if (surface_format.format == desc_.format &&
        surface_format.colorSpace == desc_.color_space) {
      selected_format = surface_format;
      break;
    }
  }

  VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
  for (auto present_mode : present_modes) {
    if (present_mode == desc_.present_mode) {
      selected_present_mode = present_mode;
      break;
    }
  }

  if (capabilities.currentExtent.width != UINT32_MAX) {
    swapchain_extent_ = capabilities.currentExtent;
  } else {
    swapchain_extent_.width =
        std::clamp(desc_.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    swapchain_extent_.height =
        std::clamp(desc_.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
  }

  swapchain_format_ = selected_format.format;
  swapchain_transform_ =
      (capabilities.supportedTransforms & desc_.pre_transform) != 0
          ? desc_.pre_transform
          : capabilities.currentTransform;

  uint32_t image_count =
      std::max(desc_.min_image_count, capabilities.minImageCount);
  if (capabilities.maxImageCount > 0) {
    image_count = std::min(image_count, capabilities.maxImageCount);
  }

  const bool shared_present_queue =
      present_queue_family_index_ != state_->GetGraphicsQueueFamilyIndex();
  const uint32_t queue_family_indices[2] = {
      static_cast<uint32_t>(state_->GetGraphicsQueueFamilyIndex()),
      static_cast<uint32_t>(present_queue_family_index_)};

  VkSwapchainCreateInfoKHR create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = desc_.surface;
  create_info.minImageCount = image_count;
  create_info.imageFormat = selected_format.format;
  create_info.imageColorSpace = selected_format.colorSpace;
  create_info.imageExtent = swapchain_extent_;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.imageSharingMode = shared_present_queue
                                     ? VK_SHARING_MODE_CONCURRENT
                                     : VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = shared_present_queue ? 2u : 0u;
  create_info.pQueueFamilyIndices =
      shared_present_queue ? queue_family_indices : nullptr;
  create_info.preTransform = swapchain_transform_;
  create_info.compositeAlpha = desc_.composite_alpha;
  create_info.presentMode = selected_present_mode;
  create_info.clipped = desc_.clipped ? VK_TRUE : VK_FALSE;
  create_info.oldSwapchain = VK_NULL_HANDLE;

  const VkResult result = fns_.vkCreateSwapchainKHR(
      state_->GetLogicalDevice(), &create_info, nullptr, &swapchain_);
  if (result != VK_SUCCESS || swapchain_ == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan swapchain: {}", static_cast<int32_t>(result));
    swapchain_ = VK_NULL_HANDLE;
    return false;
  }

  uint32_t swapchain_image_count = 0;
  if (fns_.vkGetSwapchainImagesKHR(state_->GetLogicalDevice(), swapchain_,
                                   &swapchain_image_count,
                                   nullptr) != VK_SUCCESS ||
      swapchain_image_count == 0) {
    LOGE("Failed to query Vulkan swapchain images");
    return false;
  }

  swapchain_images_.resize(swapchain_image_count);
  if (fns_.vkGetSwapchainImagesKHR(state_->GetLogicalDevice(), swapchain_,
                                   &swapchain_image_count,
                                   swapchain_images_.data()) != VK_SUCCESS) {
    LOGE("Failed to load Vulkan swapchain images");
    swapchain_images_.clear();
    return false;
  }
  swapchain_images_.resize(swapchain_image_count);
  image_in_flight_fences_.assign(swapchain_image_count, VK_NULL_HANDLE);
  return true;
}

bool GPUPresenterVK::CreateSwapchainImageViews() {
  swapchain_image_views_.reserve(swapchain_images_.size());
  for (VkImage image : swapchain_images_) {
    VkImageView image_view =
        CreateSwapchainImageView(*state_, image, swapchain_format_);
    if (image_view == VK_NULL_HANDLE) {
      return false;
    }
    swapchain_image_views_.push_back(image_view);
  }
  return true;
}

bool GPUPresenterVK::CreateFrameSlots() {
  frame_slots_.resize(std::max<size_t>(2, swapchain_images_.size()));
  for (auto& frame_slot : frame_slots_) {
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (fns_.vkCreateSemaphore(state_->GetLogicalDevice(), &semaphore_info,
                               nullptr,
                               &frame_slot.image_available) != VK_SUCCESS ||
        fns_.vkCreateSemaphore(state_->GetLogicalDevice(), &semaphore_info,
                               nullptr,
                               &frame_slot.render_finished) != VK_SUCCESS) {
      LOGE("Failed to create Vulkan presenter semaphores");
      return false;
    }

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (state_->DeviceFns().vkCreateFence(
            state_->GetLogicalDevice(), &fence_info, nullptr,
            &frame_slot.in_flight) != VK_SUCCESS) {
      LOGE("Failed to create Vulkan presenter fence");
      return false;
    }
  }
  return true;
}

void GPUPresenterVK::DestroyFrameSlots() {
  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE) {
    frame_slots_.clear();
    return;
  }

  for (auto& frame_slot : frame_slots_) {
    if (frame_slot.image_available != VK_NULL_HANDLE &&
        fns_.vkDestroySemaphore != nullptr) {
      fns_.vkDestroySemaphore(state_->GetLogicalDevice(),
                              frame_slot.image_available, nullptr);
    }
    if (frame_slot.render_finished != VK_NULL_HANDLE &&
        fns_.vkDestroySemaphore != nullptr) {
      fns_.vkDestroySemaphore(state_->GetLogicalDevice(),
                              frame_slot.render_finished, nullptr);
    }
    if (frame_slot.in_flight != VK_NULL_HANDLE &&
        state_->DeviceFns().vkDestroyFence != nullptr) {
      state_->DeviceFns().vkDestroyFence(state_->GetLogicalDevice(),
                                         frame_slot.in_flight, nullptr);
    }
  }
  frame_slots_.clear();
}

void GPUPresenterVK::DestroySwapchainImageViews() {
  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE) {
    swapchain_image_views_.clear();
    return;
  }

  for (auto image_view : swapchain_image_views_) {
    if (image_view != VK_NULL_HANDLE &&
        state_->DeviceFns().vkDestroyImageView != nullptr) {
      state_->DeviceFns().vkDestroyImageView(state_->GetLogicalDevice(),
                                             image_view, nullptr);
    }
  }
  swapchain_image_views_.clear();
}

void GPUPresenterVK::DestroySwapchain() {
  if (state_ != nullptr && state_->GetLogicalDevice() != VK_NULL_HANDLE &&
      swapchain_ != VK_NULL_HANDLE && fns_.vkDestroySwapchainKHR != nullptr) {
    fns_.vkDestroySwapchainKHR(state_->GetLogicalDevice(), swapchain_, nullptr);
  }
  swapchain_ = VK_NULL_HANDLE;
  swapchain_images_.clear();
  image_in_flight_fences_.clear();
}

void GPUPresenterVK::Reset() {
  if (state_ != nullptr && state_->GetLogicalDevice() != VK_NULL_HANDLE &&
      state_->DeviceFns().vkDeviceWaitIdle != nullptr) {
    state_->DeviceFns().vkDeviceWaitIdle(state_->GetLogicalDevice());
  }
  if (state_ != nullptr) {
    state_->CollectPendingSubmissions(true);
  }
  DestroyFrameSlots();
  DestroySwapchainImageViews();
  DestroySwapchain();
  current_frame_ = 0;
  has_outstanding_surface_ = false;
}

}  // namespace skity
