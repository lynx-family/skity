// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_WINDOW_SURFACE_VK_HPP
#define SRC_GPU_VK_GPU_WINDOW_SURFACE_VK_HPP

#include <volk.h>

#include <memory>
#include <vector>

#include "src/gpu/vk/gpu_surface_vk.hpp"

namespace skity {

class GPUDeviceVk;
class GPUTextureVk;
class VkInterface;

/**
 * @brief Vulkan surface for on-screen rendering with swapchain support
 */
class GPUWindowSurfaceVk : public GPUSurfaceVk {
 public:
  GPUWindowSurfaceVk(GPUContextImpl* ctx, uint32_t width, uint32_t height,
                     uint32_t sample_count, float content_scale);
  ~GPUWindowSurfaceVk() override;

  /**
   * Initialize the surface with a pre-created VkSurfaceKHR
   * The surface should be created using glfwCreateWindowSurface or equivalent
   */
  bool InitWithSurface(VkSurfaceKHR surface, VkInterface* vk_interface);

  /**
   * Check if the surface is valid and ready for rendering
   */
  bool IsValid() const {
    return surface_ != VK_NULL_HANDLE && swapchain_ != VK_NULL_HANDLE;
  }

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;
  void OnFlush() override;

 private:
  // Swapchain management
  bool CreateSwapchain();
  bool CreateSwapchainImageViews();
  bool CreateRenderPass();
  bool CreateFramebuffers();
  bool CreateSyncObjects();
  void CleanupSwapchain();

  // Frame management
  bool AcquireNextImage();
  bool PresentImage();

  // Query swapchain support details
  struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
  };
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

  // Choose swapchain settings
  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR>& available_formats);
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR>& available_present_modes);
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

 private:
  // Window surface
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;

  // Swapchain
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  std::vector<VkImage> swapchain_images_;
  std::vector<VkImageView> swapchain_image_views_;
  std::vector<VkFramebuffer> swapchain_framebuffers_;
  VkFormat swapchain_image_format_;
  VkExtent2D swapchain_extent_;

  // Render pass for swapchain
  VkRenderPass render_pass_ = VK_NULL_HANDLE;

  // Synchronization
  std::vector<VkSemaphore> image_available_semaphores_;
  std::vector<VkSemaphore> render_finished_semaphores_;
  std::vector<VkFence> in_flight_fences_;
  size_t current_frame_ = 0;
  uint32_t current_image_index_ = 0;

  // Maximum frames in flight
  static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

  // Device reference
  GPUDeviceVk* vk_device_ = nullptr;
  VkInstance instance_ = VK_NULL_HANDLE;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_WINDOW_SURFACE_VK_HPP