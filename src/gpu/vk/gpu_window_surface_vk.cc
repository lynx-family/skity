// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_window_surface_vk.hpp"

#include <algorithm>
#include <limits>

#include "src/gpu/vk/gpu_context_impl_vk.hpp"
#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/vk_interface.hpp"
#include "src/logging.hpp"
#include "src/render/hw/vk/vk_root_layer.hpp"

namespace skity {

GPUWindowSurfaceVk::GPUWindowSurfaceVk(GPUContextImpl* ctx, uint32_t width,
                                       uint32_t height, uint32_t sample_count,
                                       float content_scale)
    : GPUSurfaceVk(GPUSurfaceDescriptor{GPUBackendType::kVulkan, width, height,
                                        sample_count, content_scale},
                   ctx) {
  vk_device_ = static_cast<GPUDeviceVk*>(ctx->GetGPUDevice());
}

GPUWindowSurfaceVk::~GPUWindowSurfaceVk() {
  if (vk_device_ && vk_device_->GetDevice()) {
    vkDeviceWaitIdle(vk_device_->GetDevice());
  }

  CleanupSwapchain();

  // Destroy sync objects
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (render_finished_semaphores_[i]) {
      vkDestroySemaphore(vk_device_->GetDevice(),
                         render_finished_semaphores_[i], nullptr);
    }
    if (image_available_semaphores_[i]) {
      vkDestroySemaphore(vk_device_->GetDevice(),
                         image_available_semaphores_[i], nullptr);
    }
    if (in_flight_fences_[i]) {
      vkDestroyFence(vk_device_->GetDevice(), in_flight_fences_[i], nullptr);
    }
  }

  // Surface is owned and destroyed by the window, not by this class
  // Do not destroy the surface here to avoid double destruction
}

bool GPUWindowSurfaceVk::InitWithSurface(VkSurfaceKHR surface,
                                         VkInterface* vk_interface) {
  if (surface == VK_NULL_HANDLE || !vk_interface) {
    LOGE("Invalid surface or Vulkan interface");
    return false;
  }

  surface_ = surface;
  instance_ = vk_interface->GetInstance();

  // Create swapchain
  if (!CreateSwapchain()) {
    return false;
  }

  // Create image views
  if (!CreateSwapchainImageViews()) {
    return false;
  }

  // Create render pass
  if (!CreateRenderPass()) {
    return false;
  }

  // Create framebuffers
  if (!CreateFramebuffers()) {
    return false;
  }

  // Create sync objects
  if (!CreateSyncObjects()) {
    return false;
  }

  LOGI("Vulkan window surface initialized successfully");
  return true;
}

bool GPUWindowSurfaceVk::CreateSwapchain() {
  // Check for presentation support
  VkBool32 present_support = false;
  vkGetPhysicalDeviceSurfaceSupportKHR(
      vk_device_->GetPhysicalDevice(),
      vk_device_->GetQueueFamilyIndices().present_family, surface_,
      &present_support);

  if (!present_support) {
    LOGE("Surface does not support presentation");
    return false;
  }

  SwapChainSupportDetails swap_chain_support =
      QuerySwapChainSupport(vk_device_->GetPhysicalDevice());

  VkSurfaceFormatKHR surface_format =
      ChooseSwapSurfaceFormat(swap_chain_support.formats);
  VkPresentModeKHR present_mode =
      ChooseSwapPresentMode(swap_chain_support.present_modes);
  VkExtent2D extent = ChooseSwapExtent(swap_chain_support.capabilities);

  uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
  if (swap_chain_support.capabilities.maxImageCount > 0 &&
      image_count > swap_chain_support.capabilities.maxImageCount) {
    image_count = swap_chain_support.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface_;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  QueueFamilyIndices indices = vk_device_->GetQueueFamilyIndices();
  uint32_t queue_family_indices[] = {indices.graphics_family,
                                     indices.present_family};

  if (indices.graphics_family != indices.present_family) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  create_info.preTransform = swap_chain_support.capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(vk_device_->GetDevice(), &create_info, nullptr,
                           &swapchain_) != VK_SUCCESS) {
    LOGE("Failed to create swap chain");
    return false;
  }

  // Get swapchain images
  vkGetSwapchainImagesKHR(vk_device_->GetDevice(), swapchain_, &image_count,
                          nullptr);
  swapchain_images_.resize(image_count);
  vkGetSwapchainImagesKHR(vk_device_->GetDevice(), swapchain_, &image_count,
                          swapchain_images_.data());

  swapchain_image_format_ = surface_format.format;
  swapchain_extent_ = extent;

  LOGI("Swapchain created with %u images", image_count);
  return true;
}

bool GPUWindowSurfaceVk::CreateSwapchainImageViews() {
  swapchain_image_views_.resize(swapchain_images_.size());

  for (size_t i = 0; i < swapchain_images_.size(); i++) {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = swapchain_images_[i];
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = swapchain_image_format_;
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vk_device_->GetDevice(), &create_info, nullptr,
                          &swapchain_image_views_[i]) != VK_SUCCESS) {
      LOGE("Failed to create image views");
      return false;
    }
  }

  return true;
}

bool GPUWindowSurfaceVk::CreateRenderPass() {
  VkAttachmentDescription color_attachment{};
  color_attachment.format = swapchain_image_format_;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  if (vkCreateRenderPass(vk_device_->GetDevice(), &render_pass_info, nullptr,
                         &render_pass_) != VK_SUCCESS) {
    LOGE("Failed to create render pass");
    return false;
  }

  return true;
}

bool GPUWindowSurfaceVk::CreateFramebuffers() {
  swapchain_framebuffers_.resize(swapchain_image_views_.size());

  for (size_t i = 0; i < swapchain_image_views_.size(); i++) {
    VkImageView attachments[] = {swapchain_image_views_[i]};

    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = render_pass_;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = swapchain_extent_.width;
    framebuffer_info.height = swapchain_extent_.height;
    framebuffer_info.layers = 1;

    if (vkCreateFramebuffer(vk_device_->GetDevice(), &framebuffer_info, nullptr,
                            &swapchain_framebuffers_[i]) != VK_SUCCESS) {
      LOGE("Failed to create framebuffer");
      return false;
    }
  }

  return true;
}

bool GPUWindowSurfaceVk::CreateSyncObjects() {
  image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
  render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
  in_flight_fences_.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(vk_device_->GetDevice(), &semaphore_info, nullptr,
                          &image_available_semaphores_[i]) != VK_SUCCESS ||
        vkCreateSemaphore(vk_device_->GetDevice(), &semaphore_info, nullptr,
                          &render_finished_semaphores_[i]) != VK_SUCCESS ||
        vkCreateFence(vk_device_->GetDevice(), &fence_info, nullptr,
                      &in_flight_fences_[i]) != VK_SUCCESS) {
      LOGE("Failed to create synchronization objects");
      return false;
    }
  }

  return true;
}

void GPUWindowSurfaceVk::CleanupSwapchain() {
  for (auto framebuffer : swapchain_framebuffers_) {
    vkDestroyFramebuffer(vk_device_->GetDevice(), framebuffer, nullptr);
  }

  if (render_pass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(vk_device_->GetDevice(), render_pass_, nullptr);
  }

  for (auto image_view : swapchain_image_views_) {
    vkDestroyImageView(vk_device_->GetDevice(), image_view, nullptr);
  }

  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(vk_device_->GetDevice(), swapchain_, nullptr);
  }
}

bool GPUWindowSurfaceVk::AcquireNextImage() {
  // Check fence status first before waiting
  VkResult fence_status = vkGetFenceStatus(vk_device_->GetDevice(),
                                           in_flight_fences_[current_frame_]);

  VkResult fence_result = vkWaitForFences(
      vk_device_->GetDevice(), 1, &in_flight_fences_[current_frame_], VK_TRUE,
      100000000ULL);  // 0.1 second timeout - shorter to avoid blocking
  if (fence_result == VK_TIMEOUT) {
    LOGE("Fence wait timed out after 0.1 seconds - frame may be stuck");
    return false;
  } else if (fence_result != VK_SUCCESS) {
    LOGE("Fence wait failed: %d", fence_result);
    return false;
  }
  LOGI("AcquireNextImage: Fence wait completed, acquiring image");

  VkResult result = vkAcquireNextImageKHR(
      vk_device_->GetDevice(), swapchain_, 100000000ULL,  // 0.1 second timeout
      image_available_semaphores_[current_frame_], VK_NULL_HANDLE,
      &current_image_index_);
  LOGI("AcquireNextImage: vkAcquireNextImageKHR result: %d, image_index: %u",
       result, current_image_index_);

  if (result == VK_TIMEOUT) {
    LOGE("Swapchain image acquisition timed out");
    return false;
  } else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    LOGE("Swapchain out of date - recreation needed");
    return false;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    LOGE("Failed to acquire swap chain image: %d", result);
    return false;
  }

  // Only reset fence after successful acquisition
  vkResetFences(vk_device_->GetDevice(), 1, &in_flight_fences_[current_frame_]);
  LOGI("AcquireNextImage: Success, image_index: %u", current_image_index_);

  return true;
}

bool GPUWindowSurfaceVk::PresentImage() {
  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  VkSemaphore wait_semaphores[] = {render_finished_semaphores_[current_frame_]};
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = wait_semaphores;

  VkSwapchainKHR swap_chains[] = {swapchain_};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swap_chains;
  present_info.pImageIndices = &current_image_index_;

  VkResult result =
      vkQueuePresentKHR(vk_device_->GetPresentQueue(), &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // TODO: Handle swapchain recreation
    return false;
  } else if (result != VK_SUCCESS) {
    LOGE("Failed to present swap chain image");
    return false;
  }

  current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;

  return true;
}

GPUWindowSurfaceVk::SwapChainSupportDetails
GPUWindowSurfaceVk::QuerySwapChainSupport(VkPhysicalDevice device) {
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_,
                                            &details.capabilities);

  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count,
                                       nullptr);

  if (format_count != 0) {
    details.formats.resize(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count,
                                         details.formats.data());
  }

  uint32_t present_mode_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_,
                                            &present_mode_count, nullptr);

  if (present_mode_count != 0) {
    details.present_modes.resize(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface_, &present_mode_count, details.present_modes.data());
  }

  return details;
}

VkSurfaceFormatKHR GPUWindowSurfaceVk::ChooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& available_formats) {
  // Prefer linear (UNORM) format to match GL behavior and avoid sRGB conversion
  // issues
  for (const auto& available_format : available_formats) {
    if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return available_format;
    }
  }

  // Fall back to RGBA UNORM if BGRA UNORM not available
  for (const auto& available_format : available_formats) {
    if (available_format.format == VK_FORMAT_R8G8B8A8_UNORM &&
        available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return available_format;
    }
  }

  // Last resort: try sRGB format (will appear lighter due to gamma correction)
  for (const auto& available_format : available_formats) {
    if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return available_format;
    }
  }

  return available_formats[0];
}

VkPresentModeKHR GPUWindowSurfaceVk::ChooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& available_present_modes) {
  for (const auto& available_present_mode : available_present_modes) {
    if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return available_present_mode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D GPUWindowSurfaceVk::ChooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    VkExtent2D actual_extent = {GetWidth(), GetHeight()};

    actual_extent.width = std::max(
        capabilities.minImageExtent.width,
        std::min(capabilities.maxImageExtent.width, actual_extent.width));
    actual_extent.height = std::max(
        capabilities.minImageExtent.height,
        std::min(capabilities.maxImageExtent.height, actual_extent.height));

    return actual_extent;
  }
}

HWRootLayer* GPUWindowSurfaceVk::OnBeginNextFrame(bool clear) {
  // Acquire next swapchain image
  if (!AcquireNextImage()) {
    LOGE("Failed to acquire next swapchain image");
    return nullptr;
  }

  // Create a texture wrapper for the current swapchain image
  GPUTextureDescriptor tex_desc;
  tex_desc.format =
      GPUTextureFormat::kRGBA8Unorm;  // Convert from Vulkan format
  tex_desc.width = swapchain_extent_.width;
  tex_desc.height = swapchain_extent_.height;
  tex_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);

  // Create a texture wrapper around the current swapchain image
  auto* device_vk = static_cast<GPUDeviceVk*>(GetGPUContext()->GetGPUDevice());
  VkImage current_swapchain_image = swapchain_images_[current_image_index_];
  VkFormat swapchain_format = swapchain_image_format_;

  auto swapchain_texture = GPUTextureVk::CreateFromVkImage(
      device_vk, current_swapchain_image, swapchain_format,
      swapchain_extent_.width, swapchain_extent_.height);

  if (!swapchain_texture) {
    LOGE("Failed to create swapchain texture wrapper");
    return nullptr;
  }

  // Create root layer for this frame
  auto root_layer = GetArenaAllocator()->Make<VkExternTextureLayer>(
      swapchain_texture, Rect::MakeWH(GetWidth(), GetHeight()));

  root_layer->SetClearSurface(clear);
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());

  return root_layer;
}

void GPUWindowSurfaceVk::OnFlush() {
  // CRITICAL: Flush the canvas to execute all drawing commands
  // The canvas rendering happens to the swapchain image via
  // VkExternTextureLayer
  FlushCanvas();

  // Canvas rendering is complete. The canvas command submission already
  // waited for GPU completion via vkQueueWaitIdle(), so the swapchain
  // image now contains the rendered content.

  // Signal the fence for next frame synchronization
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &image_available_semaphores_[current_frame_];

  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submit_info.pWaitDstStageMask = wait_stages;

  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &render_finished_semaphores_[current_frame_];

  // Submit empty command buffer for synchronization only
  submit_info.commandBufferCount = 0;
  submit_info.pCommandBuffers = nullptr;

  VkResult result =
      vkQueueSubmit(vk_device_->GetGraphicsQueue(), 1, &submit_info,
                    in_flight_fences_[current_frame_]);
  if (result != VK_SUCCESS) {
    LOGE("Failed to submit presentation synchronization: %d", result);
  }

  // Present the image
  if (!PresentImage()) {
    LOGE("Failed to present image");
  }
}

}  // namespace skity