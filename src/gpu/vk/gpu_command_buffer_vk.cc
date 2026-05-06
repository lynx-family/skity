// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_command_buffer_vk.hpp"

#include "src/gpu/vk/gpu_blit_pass_vk.hpp"
#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_render_pass_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

#if defined(SKITY_VK_DEBUG_RUNTIME)

void SetVkObjectDebugLabel(const VulkanContextState& state,
                           VkObjectType object_type, uint64_t object_handle,
                           const std::string& label) {
  if (label.empty() || object_handle == 0 ||
      !state.HasEnabledInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) ||
      state.GetLogicalDevice() == VK_NULL_HANDLE ||
      state.DeviceFns().vkSetDebugUtilsObjectNameEXT == nullptr) {
    return;
  }

  VkDebugUtilsObjectNameInfoEXT name_info = {};
  name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  name_info.objectType = object_type;
  name_info.objectHandle = object_handle;
  name_info.pObjectName = label.c_str();

  const VkResult result = state.DeviceFns().vkSetDebugUtilsObjectNameEXT(
      state.GetLogicalDevice(), &name_info);
  if (result != VK_SUCCESS) {
    LOGW("Failed to set Vulkan object debug label '{}': {}", label,
         static_cast<int32_t>(result));
  }
}

#else

void SetVkObjectDebugLabel(const VulkanContextState& state,
                           VkObjectType object_type, uint64_t object_handle,
                           const std::string& label) {
  (void)state;
  (void)object_type;
  (void)object_handle;
  (void)label;
}

#endif

}  // namespace

VkResult SubmitCommandBuffer(const VulkanContextState& state, VkQueue queue,
                             VkCommandBuffer command_buffer, VkFence fence,
                             const GPUSubmitInfoVK* submit_info) {
  const auto& device_fns = state.DeviceFns();
  if (state.IsSynchronization2Enabled() &&
      device_fns.vkQueueSubmit2 != nullptr) {
    VkCommandBufferSubmitInfo command_buffer_info = {};
    command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_info.commandBuffer = command_buffer;
    command_buffer_info.deviceMask = 0;

    VkSemaphoreSubmitInfo wait_semaphore_info = {};
    if (submit_info != nullptr &&
        submit_info->wait_semaphore != VK_NULL_HANDLE) {
      wait_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      wait_semaphore_info.semaphore = submit_info->wait_semaphore;
      wait_semaphore_info.stageMask = submit_info->wait_dst_stage_mask;
      wait_semaphore_info.deviceIndex = 0;
      wait_semaphore_info.value = 0;
    }

    VkSemaphoreSubmitInfo signal_semaphore_info = {};
    if (submit_info != nullptr &&
        submit_info->signal_semaphore != VK_NULL_HANDLE) {
      signal_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      signal_semaphore_info.semaphore = submit_info->signal_semaphore;
      signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
      signal_semaphore_info.deviceIndex = 0;
      signal_semaphore_info.value = 0;
    }

    VkSubmitInfo2 submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    if (wait_semaphore_info.semaphore != VK_NULL_HANDLE) {
      submit_info.waitSemaphoreInfoCount = 1;
      submit_info.pWaitSemaphoreInfos = &wait_semaphore_info;
    }
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_buffer_info;
    if (signal_semaphore_info.semaphore != VK_NULL_HANDLE) {
      submit_info.signalSemaphoreInfoCount = 1;
      submit_info.pSignalSemaphoreInfos = &signal_semaphore_info;
    }

    return device_fns.vkQueueSubmit2(queue, 1, &submit_info, fence);
  }

  VkPipelineStageFlags wait_stage_mask =
      submit_info != nullptr ? submit_info->wait_dst_stage_mask
                             : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo vk_submit_info = {};
  vk_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  if (submit_info != nullptr && submit_info->wait_semaphore != VK_NULL_HANDLE) {
    vk_submit_info.waitSemaphoreCount = 1;
    vk_submit_info.pWaitSemaphores = &submit_info->wait_semaphore;
    vk_submit_info.pWaitDstStageMask = &wait_stage_mask;
  }
  vk_submit_info.commandBufferCount = 1;
  vk_submit_info.pCommandBuffers = &command_buffer;
  if (submit_info != nullptr &&
      submit_info->signal_semaphore != VK_NULL_HANDLE) {
    vk_submit_info.signalSemaphoreCount = 1;
    vk_submit_info.pSignalSemaphores = &submit_info->signal_semaphore;
  }

  return device_fns.vkQueueSubmit(queue, 1, &vk_submit_info, fence);
}

GPUCommandBufferVK::GPUCommandBufferVK(
    std::shared_ptr<const VulkanContextState> state)
    : state_(std::move(state)) {}

GPUCommandBufferVK::~GPUCommandBufferVK() { Reset(); }

bool GPUCommandBufferVK::Init() {
  if (state_ != nullptr) {
    state_->CollectPendingSubmissions(false);
  }

  if (state_ == nullptr || state_->GetLogicalDevice() == VK_NULL_HANDLE ||
      state_->GetGraphicsQueue() == VK_NULL_HANDLE ||
      state_->GetGraphicsQueueFamilyIndex() < 0) {
    LOGE("Failed to initialize Vulkan command buffer: context is unavailable");
    return false;
  }

  const auto& device_fns = state_->DeviceFns();
  VkCommandPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pool_info.queueFamilyIndex =
      static_cast<uint32_t>(state_->GetGraphicsQueueFamilyIndex());

  VkResult result = device_fns.vkCreateCommandPool(
      state_->GetLogicalDevice(), &pool_info, nullptr, &command_pool_);
  if (result != VK_SUCCESS || command_pool_ == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan command pool: {}",
         static_cast<int32_t>(result));
    Reset();
    return false;
  }

  VkCommandBufferAllocateInfo allocate_info = {};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = command_pool_;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1;

  result = device_fns.vkAllocateCommandBuffers(
      state_->GetLogicalDevice(), &allocate_info, &command_buffer_);
  if (result != VK_SUCCESS || command_buffer_ == VK_NULL_HANDLE) {
    LOGE("Failed to allocate Vulkan command buffer: {}",
         static_cast<int32_t>(result));
    Reset();
    return false;
  }

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  result = device_fns.vkBeginCommandBuffer(command_buffer_, &begin_info);
  if (result != VK_SUCCESS) {
    LOGE("Failed to begin Vulkan command buffer: {}",
         static_cast<int32_t>(result));
    Reset();
    return false;
  }

  recording_ = true;
  return true;
}

std::shared_ptr<GPURenderPass> GPUCommandBufferVK::BeginRenderPass(
    const GPURenderPassDescriptor& desc) {
  if (!recording_ || command_buffer_ == VK_NULL_HANDLE || state_ == nullptr) {
    LOGE("Failed to begin Vulkan render pass: command buffer is not recording");
    return {};
  }

  return std::make_shared<GPURenderPassVK>(state_, this, desc);
}

std::shared_ptr<GPUBlitPass> GPUCommandBufferVK::BeginBlitPass() {
  if (!recording_ || command_buffer_ == VK_NULL_HANDLE) {
    LOGE("Failed to begin Vulkan blit pass: command buffer is not recording");
    return {};
  }

  return std::make_shared<GPUBlitPassVK>(state_, this);
}

void GPUCommandBufferVK::HoldResource(std::shared_ptr<void> resource) {
  if (resource == nullptr) {
    return;
  }

  cleanup_actions_.emplace_back([resource = std::move(resource)]() {});
}

bool GPUCommandBufferVK::Submit(const GPUSubmitInfo* submit_info) {
  if (!recording_ || submitted_ || state_ == nullptr ||
      command_buffer_ == VK_NULL_HANDLE) {
    return false;
  }

  if (submit_info != nullptr &&
      submit_info->GetBackendType() != GPUBackendType::kVulkan) {
    LOGE("Failed to submit Vulkan command buffer: invalid submit info type");
    return false;
  }
  const auto* submit_info_vk = static_cast<const GPUSubmitInfoVK*>(submit_info);

  ApplyDebugLabelsIfNeeded();

  const auto& device_fns = state_->DeviceFns();
  VkFence fence =
      submit_info_vk != nullptr ? submit_info_vk->signal_fence : VK_NULL_HANDLE;
  bool owns_fence = false;

  VkResult result = device_fns.vkEndCommandBuffer(command_buffer_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to end Vulkan command buffer: {}",
         static_cast<int32_t>(result));
    return false;
  }

  recording_ = false;

  if (fence == VK_NULL_HANDLE) {
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    result = device_fns.vkCreateFence(state_->GetLogicalDevice(), &fence_info,
                                      nullptr, &fence);
    if (result != VK_SUCCESS || fence == VK_NULL_HANDLE) {
      LOGE("Failed to create Vulkan fence: {}", static_cast<int32_t>(result));
      return false;
    }
    owns_fence = true;
  }

  result = SubmitCommandBuffer(*state_, state_->GetGraphicsQueue(),
                               command_buffer_, fence, submit_info_vk);
  if (result != VK_SUCCESS) {
    LOGE("Failed to submit Vulkan command buffer: {}",
         static_cast<int32_t>(result));
    if (owns_fence) {
      device_fns.vkDestroyFence(state_->GetLogicalDevice(), fence, nullptr);
    }
    return false;
  }

  state_->EnqueuePendingSubmission(
      VulkanPendingSubmission(fence, command_pool_, std::move(stage_buffers_),
                              std::move(cleanup_actions_), owns_fence));
  state_->CollectPendingSubmissions(false);

  submitted_ = true;
  command_buffer_ = VK_NULL_HANDLE;
  command_pool_ = VK_NULL_HANDLE;
  return true;
}

void GPUCommandBufferVK::RecordStageBuffer(
    std::unique_ptr<GPUBufferVK> buffer) {
  if (buffer != nullptr) {
    stage_buffers_.emplace_back(std::move(buffer));
  }
}

void GPUCommandBufferVK::RecordCleanupAction(std::function<void()> action) {
  if (action) {
    cleanup_actions_.emplace_back(std::move(action));
  }
}

void GPUCommandBufferVK::ApplyDebugLabelsIfNeeded() {
  if (state_ == nullptr) {
    return;
  }

  const std::string& label = GetLabel();
  if (label.empty()) {
    return;
  }

  SetVkObjectDebugLabel(*state_, VK_OBJECT_TYPE_COMMAND_POOL,
                        reinterpret_cast<uint64_t>(command_pool_),
                        label + " CommandPool");
  SetVkObjectDebugLabel(*state_, VK_OBJECT_TYPE_COMMAND_BUFFER,
                        reinterpret_cast<uint64_t>(command_buffer_), label);
}

void GPUCommandBufferVK::Reset() {
  stage_buffers_.clear();
  cleanup_actions_.clear();

  if (state_ != nullptr && command_pool_ != VK_NULL_HANDLE &&
      state_->GetLogicalDevice() != VK_NULL_HANDLE &&
      state_->DeviceFns().vkDestroyCommandPool != nullptr) {
    state_->DeviceFns().vkDestroyCommandPool(state_->GetLogicalDevice(),
                                             command_pool_, nullptr);
  }

  command_buffer_ = VK_NULL_HANDLE;
  command_pool_ = VK_NULL_HANDLE;
  recording_ = false;
  submitted_ = false;
}

}  // namespace skity
