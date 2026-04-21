// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_command_buffer_vk.hpp"

#include "src/gpu/vk/gpu_blit_pass_vk.hpp"
#include "src/gpu/vk/gpu_buffer_vk.hpp"
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
  (void)desc;
  LOGW("GPUCommandBufferVK::BeginRenderPass is not implemented yet");
  return {};
}

std::shared_ptr<GPUBlitPass> GPUCommandBufferVK::BeginBlitPass() {
  if (!recording_ || command_buffer_ == VK_NULL_HANDLE) {
    LOGE("Failed to begin Vulkan blit pass: command buffer is not recording");
    return {};
  }

  return std::make_shared<GPUBlitPassVK>(state_, this);
}

bool GPUCommandBufferVK::Submit() {
  if (!recording_ || submitted_ || state_ == nullptr ||
      command_buffer_ == VK_NULL_HANDLE) {
    return false;
  }

  ApplyDebugLabelsIfNeeded();

  const auto& device_fns = state_->DeviceFns();
  VkFence fence = VK_NULL_HANDLE;

  VkResult result = device_fns.vkEndCommandBuffer(command_buffer_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to end Vulkan command buffer: {}",
         static_cast<int32_t>(result));
    return false;
  }

  recording_ = false;

  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  result =
      device_fns.vkCreateFence(state_->GetLogicalDevice(), &fence_info, nullptr,
                               &fence);
  if (result != VK_SUCCESS || fence == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan fence: {}", static_cast<int32_t>(result));
    return false;
  }

  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer_;

  result =
      device_fns.vkQueueSubmit(state_->GetGraphicsQueue(), 1, &submit_info,
                               fence);
  if (result != VK_SUCCESS) {
    LOGE("Failed to submit Vulkan command buffer: {}",
         static_cast<int32_t>(result));
    device_fns.vkDestroyFence(state_->GetLogicalDevice(), fence, nullptr);
    return false;
  }

  state_->EnqueuePendingSubmission(VulkanPendingSubmission(
      fence, command_pool_, std::move(stage_buffers_)));
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
