// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/vulkan_render_pass_cache.hpp"

#include <array>

#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

constexpr size_t kInitialRenderPassCacheCapacity = 8;

}  // namespace

VulkanRenderPassCache::VulkanRenderPassCache() {
  // Legacy render pass variants are expected to stay small in practice.
  render_passes_.reserve(kInitialRenderPassCacheCapacity);
}

bool VulkanRenderPassCache::Key::operator==(const Key& other) const {
  return color_format == other.color_format &&
         color_samples == other.color_samples &&
         color_load_op == other.color_load_op &&
         color_store_op == other.color_store_op &&
         has_resolve == other.has_resolve &&
         resolve_format == other.resolve_format &&
         resolve_store_op == other.resolve_store_op &&
         has_depth == other.has_depth && has_stencil == other.has_stencil &&
         depth_stencil_format == other.depth_stencil_format &&
         depth_stencil_samples == other.depth_stencil_samples &&
         depth_load_op == other.depth_load_op &&
         depth_store_op == other.depth_store_op &&
         stencil_load_op == other.stencil_load_op &&
         stencil_store_op == other.stencil_store_op;
}

VkRenderPass VulkanRenderPassCache::GetOrCreate(
    const VulkanContextState& state, const Key& key) {
  if (state.GetLogicalDevice() == VK_NULL_HANDLE ||
      state.DeviceFns().vkCreateRenderPass == nullptr) {
    return VK_NULL_HANDLE;
  }

  for (const auto& entry : render_passes_) {
    if (entry.key == key) {
      return entry.render_pass;
    }
  }

  std::array<VkAttachmentDescription, 3> attachment_descs = {};
  uint32_t attachment_count = 0;

  VkAttachmentReference color_ref = {};
  attachment_descs[attachment_count].format = key.color_format;
  attachment_descs[attachment_count].samples = key.color_samples;
  attachment_descs[attachment_count].loadOp = key.color_load_op;
  attachment_descs[attachment_count].storeOp = key.color_store_op;
  attachment_descs[attachment_count].stencilLoadOp =
      VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment_descs[attachment_count].stencilStoreOp =
      VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment_descs[attachment_count].initialLayout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  attachment_descs[attachment_count].finalLayout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_ref.attachment = attachment_count;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  ++attachment_count;

  VkAttachmentReference resolve_ref = {};
  const VkAttachmentReference* resolve_ref_ptr = nullptr;
  if (key.has_resolve) {
    attachment_descs[attachment_count].format = key.resolve_format;
    attachment_descs[attachment_count].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[attachment_count].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[attachment_count].storeOp = key.resolve_store_op;
    attachment_descs[attachment_count].stencilLoadOp =
        VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[attachment_count].stencilStoreOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[attachment_count].initialLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment_descs[attachment_count].finalLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    resolve_ref.attachment = attachment_count;
    resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    resolve_ref_ptr = &resolve_ref;
    ++attachment_count;
  }

  VkAttachmentReference depth_stencil_ref = {};
  const VkAttachmentReference* depth_stencil_ref_ptr = nullptr;
  if (key.has_depth || key.has_stencil) {
    attachment_descs[attachment_count].format = key.depth_stencil_format;
    attachment_descs[attachment_count].samples = key.depth_stencil_samples;
    attachment_descs[attachment_count].loadOp =
        key.has_depth ? key.depth_load_op : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[attachment_count].storeOp =
        key.has_depth ? key.depth_store_op : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[attachment_count].stencilLoadOp = key.stencil_load_op;
    attachment_descs[attachment_count].stencilStoreOp = key.stencil_store_op;
    attachment_descs[attachment_count].initialLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachment_descs[attachment_count].finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_stencil_ref.attachment = attachment_count;
    depth_stencil_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_stencil_ref_ptr = &depth_stencil_ref;
    ++attachment_count;
  }

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;
  subpass.pResolveAttachments = resolve_ref_ptr;
  subpass.pDepthStencilAttachment = depth_stencil_ref_ptr;

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = attachment_count;
  render_pass_info.pAttachments = attachment_descs.data();
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;

  VkRenderPass render_pass = VK_NULL_HANDLE;
  const VkResult result = state.DeviceFns().vkCreateRenderPass(
      state.GetLogicalDevice(), &render_pass_info, nullptr, &render_pass);
  if (result != VK_SUCCESS || render_pass == VK_NULL_HANDLE) {
    LOGE("Failed to create cached Vulkan render pass: {}",
         static_cast<int32_t>(result));
    return VK_NULL_HANDLE;
  }

  render_passes_.push_back({key, render_pass});
  return render_pass;
}

void VulkanRenderPassCache::Reset(const VulkanContextState& state) {
  if (state.GetLogicalDevice() != VK_NULL_HANDLE &&
      state.DeviceFns().vkDestroyRenderPass != nullptr) {
    for (const auto& entry : render_passes_) {
      if (entry.render_pass != VK_NULL_HANDLE) {
        state.DeviceFns().vkDestroyRenderPass(state.GetLogicalDevice(),
                                              entry.render_pass, nullptr);
      }
    }
  }

  render_passes_.clear();
}

}  // namespace skity
