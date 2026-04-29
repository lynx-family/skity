// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_PRESENTER_VK_HPP
#define SRC_GPU_VK_GPU_PRESENTER_VK_HPP

#include <vulkan/vulkan.h>

#include <memory>
#include <skity/gpu/gpu_context_vk.hpp>
#include <skity/gpu/gpu_presenter.hpp>
#include <vector>

#include "src/gpu/vk/gpu_surface_vk.hpp"

namespace skity {

class GPUContextImpl;
class VulkanContextState;

class GPUPresenterVK : public GPUPresenter {
 public:
  GPUPresenterVK(GPUContextImpl* context,
                 std::shared_ptr<const VulkanContextState> state,
                 const GPUPresenterDescriptorVK& desc);

  ~GPUPresenterVK() override;

  bool Init();

  GPUSurfaceAcquireResult AcquireNextSurface(
      const GPUSurfaceAcquireDescriptor& desc) override;

  GPUPresenterStatus Present(std::unique_ptr<GPUSurface> surface) override;

 private:
  struct FrameSlot {
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence in_flight = VK_NULL_HANDLE;
  };

  struct PresenterFns {
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
        vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
        vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR
        vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;
    PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
    PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
    PFN_vkResetFences vkResetFences = nullptr;
  };

  struct SurfacePresentInfo {
    const GPUPresenterVK* owner = nullptr;
    uint32_t image_index = 0;
  };

  bool LoadPresenterFns();
  bool CreateSwapchain();
  bool CreateSwapchainImageViews();
  bool CreateFrameSlots();
  void DestroyFrameSlots();
  void DestroySwapchainImageViews();
  void DestroySwapchain();
  void Reset();

  GPUContextImpl* context_ = nullptr;
  std::shared_ptr<const VulkanContextState> state_ = {};
  GPUPresenterDescriptorVK desc_ = {};
  PresenterFns fns_ = {};
  VkQueue present_queue_ = VK_NULL_HANDLE;
  int32_t present_queue_family_index_ = -1;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkExtent2D swapchain_extent_ = {};
  VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
  VkSurfaceTransformFlagBitsKHR swapchain_transform_ =
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  std::vector<VkImage> swapchain_images_ = {};
  std::vector<VkImageView> swapchain_image_views_ = {};
  std::vector<FrameSlot> frame_slots_ = {};
  std::vector<VkFence> image_in_flight_fences_ = {};
  uint32_t current_frame_ = 0;
  bool has_outstanding_surface_ = false;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_PRESENTER_VK_HPP
