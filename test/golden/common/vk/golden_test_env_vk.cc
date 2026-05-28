// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/vk/golden_test_env_vk.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <iostream>
#include <skity/gpu/gpu_context_vk.hpp>
#include <vector>

#include "common/golden_test_env.hpp"
#include "common/vk/golden_texture_vk.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/vk/gpu_context_impl_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"

namespace skity {
namespace testing {

GoldenTestEnvVK::GoldenTestEnvVK() = default;

GoldenTestEnvVK::~GoldenTestEnvVK() = default;

void GoldenTestEnvVK::SetUp() { GoldenTestEnv::SetUp(); }

void GoldenTestEnvVK::TearDown() { GoldenTestEnv::TearDown(); }

std::unique_ptr<GPUContext> GoldenTestEnvVK::CreateGPUContext() {
  GPUContextInfoVK info = {};
  info.get_instance_proc_addr = vkGetInstanceProcAddr;
  return CreateGPUContextVK(&info);
}

static bool ReadPixelsFromVulkanImage(const VulkanContextState* state,
                                      VkImage image,
                                      VkImageLayout current_layout,
                                      uint32_t width, uint32_t height,
                                      std::vector<uint8_t>* pixels) {
  VkDevice device = state->GetLogicalDevice();
  VkDeviceSize buffer_size = width * height * 4;

  // Load device functions not present in DeviceFns()
  auto fn_vkCmdCopyImageToBuffer =
      reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(state->GetInstanceProcAddr()(
          state->GetInstance(), "vkCmdCopyImageToBuffer"));

  // Create host-visible staging buffer
  VkBufferCreateInfo buffer_info = {};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = buffer_size;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

  VkBuffer staging_buffer = VK_NULL_HANDLE;
  VmaAllocation staging_allocation = VK_NULL_HANDLE;
  VkResult result =
      vmaCreateBuffer(state->GetAllocator(), &buffer_info, &alloc_info,
                      &staging_buffer, &staging_allocation, nullptr);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create staging buffer: " << result << std::endl;
    return false;
  }

  // Create command pool
  VkCommandPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex =
      static_cast<uint32_t>(state->GetGraphicsQueueFamilyIndex());
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

  VkCommandPool command_pool = VK_NULL_HANDLE;
  result = state->DeviceFns().vkCreateCommandPool(device, &pool_info, nullptr,
                                                  &command_pool);
  if (result != VK_SUCCESS) {
    vmaDestroyBuffer(state->GetAllocator(), staging_buffer, staging_allocation);
    std::cerr << "Failed to create command pool: " << result << std::endl;
    return false;
  }

  // Allocate command buffer
  VkCommandBufferAllocateInfo cmd_alloc_info = {};
  cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_alloc_info.commandPool = command_pool;
  cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_alloc_info.commandBufferCount = 1;

  VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
  result = state->DeviceFns().vkAllocateCommandBuffers(device, &cmd_alloc_info,
                                                       &cmd_buffer);
  if (result != VK_SUCCESS) {
    state->DeviceFns().vkDestroyCommandPool(device, command_pool, nullptr);
    vmaDestroyBuffer(state->GetAllocator(), staging_buffer, staging_allocation);
    std::cerr << "Failed to allocate command buffer: " << result << std::endl;
    return false;
  }

  // Record commands
  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  state->DeviceFns().vkBeginCommandBuffer(cmd_buffer, &begin_info);

  // Transition image to TRANSFER_SRC layout
  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  barrier.oldLayout = current_layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;

  state->DeviceFns().vkCmdPipelineBarrier(
      cmd_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  // Copy image to buffer
  VkBufferImageCopy copy_region = {};
  copy_region.bufferOffset = 0;
  copy_region.bufferRowLength = 0;
  copy_region.bufferImageHeight = 0;
  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageOffset = {0, 0, 0};
  copy_region.imageExtent = {width, height, 1};

  fn_vkCmdCopyImageToBuffer(cmd_buffer, image,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            staging_buffer, 1, &copy_region);

  state->DeviceFns().vkEndCommandBuffer(cmd_buffer);

  // Submit and wait
  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd_buffer;

  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence = VK_NULL_HANDLE;
  state->DeviceFns().vkCreateFence(device, &fence_info, nullptr, &fence);

  state->DeviceFns().vkQueueSubmit(state->GetGraphicsQueue(), 1, &submit_info,
                                   fence);
  state->DeviceFns().vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

  // Map and read pixel data
  void* mapped_data = nullptr;
  vmaMapMemory(state->GetAllocator(), staging_allocation, &mapped_data);

  pixels->resize(buffer_size);
  std::memcpy(pixels->data(), mapped_data, buffer_size);

  vmaUnmapMemory(state->GetAllocator(), staging_allocation);

  // Cleanup
  state->DeviceFns().vkDestroyFence(device, fence, nullptr);
  // vkFreeCommandBuffers is not needed: destroying the pool frees its buffers
  state->DeviceFns().vkDestroyCommandPool(device, command_pool, nullptr);
  vmaDestroyBuffer(state->GetAllocator(), staging_buffer, staging_allocation);

  return true;
}

std::shared_ptr<GoldenTexture> GoldenTestEnvVK::RenderToTexture(
    uint32_t width, uint32_t height,
    const std::function<void(Canvas*)>& render) {
  auto* vk_context = static_cast<GPUContextVK*>(GetGPUContext());
  auto* state = vk_context->GetState();
  auto* device = static_cast<GPUContextImpl*>(vk_context)->GetGPUDevice();

  // Create color texture for rendering
  GPUTextureDescriptor color_desc = {};
  color_desc.width = width;
  color_desc.height = height;
  color_desc.mip_level_count = 1;
  color_desc.sample_count = 1;
  color_desc.format = GPUTextureFormat::kRGBA8Unorm;
  color_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment) |
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopySrc) |
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
  color_desc.storage_mode = GPUTextureStorageMode::kPrivate;

  auto color_texture = device->CreateTexture(color_desc);
  if (!color_texture) {
    std::cerr << "Failed to create Vulkan color texture" << std::endl;
    return {};
  }

  auto* vk_texture = GPUTextureVK::Cast(color_texture.get());

  // Query max supported sample count for MSAA
  uint32_t surface_sample_count = 1;
  {
    VkImageFormatProperties format_props = {};
    VkResult query_result =
        state->InstanceFns().vkGetPhysicalDeviceImageFormatProperties(
            state->GetPhysicalDevice(), VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &format_props);
    if (query_result == VK_SUCCESS &&
        (format_props.sampleCounts & VK_SAMPLE_COUNT_4_BIT) != 0) {
      surface_sample_count = GetSampleCount();
    }
  }

  // Create GPUSurfaceDescriptorVK
  GPUSurfaceDescriptorVK surface_desc = {};
  surface_desc.backend = GPUBackendType::kVulkan;
  surface_desc.width = width;
  surface_desc.height = height;
  surface_desc.sample_count = surface_sample_count;
  surface_desc.surface_type = VKSurfaceType::kTexture;
  surface_desc.image = vk_texture->GetImage();
  surface_desc.image_view = vk_texture->GetImageView();
  surface_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
  surface_desc.image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                             VK_IMAGE_USAGE_SAMPLED_BIT;
  surface_desc.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  surface_desc.final_layout = VK_IMAGE_LAYOUT_GENERAL;
  surface_desc.owns_image = false;
  surface_desc.owns_image_view = false;

  auto surface = GetGPUContext()->CreateSurface(&surface_desc);
  if (!surface) {
    std::cerr << "Failed to create Vulkan GPU surface" << std::endl;
    return {};
  }

  auto canvas = surface->LockCanvas();
  render(canvas);
  canvas->Flush();
  surface->Flush();

  // Wait for rendering to complete
  state->CollectPendingSubmissions(true);

  // Read pixels from the rendered texture
  std::vector<uint8_t> pixels;
  if (!ReadPixelsFromVulkanImage(state, vk_texture->GetImage(),
                                 surface_desc.final_layout, width, height,
                                 &pixels)) {
    std::cerr << "Failed to read pixels from Vulkan texture" << std::endl;
    return {};
  }

  auto data = Data::MakeWithCopy(pixels.data(), pixels.size());
  auto pixmap = std::make_shared<Pixmap>(std::move(data), width, height,
                                         AlphaType::kPremul_AlphaType);
  auto image = skity::Image::MakeImage(pixmap, nullptr);

  return std::make_shared<GoldenTextureVK>(std::move(image), std::move(pixmap));
}

GoldenTestEnv* CreateGoldenTestEnvVK() { return new GoldenTestEnvVK(); }

}  // namespace testing
}  // namespace skity
