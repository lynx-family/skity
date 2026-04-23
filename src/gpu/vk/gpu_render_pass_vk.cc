// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_render_pass_vk.hpp"

#include <array>

#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

bool IsDepthStencilFormat(GPUTextureFormat format) {
  return format == GPUTextureFormat::kStencil8 ||
         format == GPUTextureFormat::kDepth24Stencil8;
}

bool HasDepthAspect(GPUTextureFormat format) {
  return format == GPUTextureFormat::kDepth24Stencil8;
}

bool HasStencilAspect(GPUTextureFormat format) {
  return format == GPUTextureFormat::kStencil8 ||
         format == GPUTextureFormat::kDepth24Stencil8;
}

VkImageAspectFlags GetImageAspectMask(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kStencil8:
      return VK_IMAGE_ASPECT_STENCIL_BIT;
    case GPUTextureFormat::kDepth24Stencil8:
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case GPUTextureFormat::kInvalid:
      return 0;
    default:
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

VkAccessFlags AccessMaskForLayout(VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
      return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    default:
      return 0;
  }
}

VkPipelineStageFlags StageMaskForLayout(VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
      return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    default:
      return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }
}

VkFormat ToVkFormat(GPUTextureFormat format) {
  switch (format) {
    case GPUTextureFormat::kR8Unorm:
      return VK_FORMAT_R8_UNORM;
    case GPUTextureFormat::kRGB8Unorm:
      return VK_FORMAT_R8G8B8_UNORM;
    case GPUTextureFormat::kRGB565Unorm:
      return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case GPUTextureFormat::kRGBA8Unorm:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case GPUTextureFormat::kBGRA8Unorm:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case GPUTextureFormat::kStencil8:
      return VK_FORMAT_S8_UINT;
    case GPUTextureFormat::kDepth24Stencil8:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case GPUTextureFormat::kInvalid:
      return VK_FORMAT_UNDEFINED;
  }

  return VK_FORMAT_UNDEFINED;
}

VkAttachmentLoadOp ToVkLoadOp(GPULoadOp op) {
  switch (op) {
    case GPULoadOp::kClear:
      return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case GPULoadOp::kLoad:
      return VK_ATTACHMENT_LOAD_OP_LOAD;
    case GPULoadOp::kDontCare:
      return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  }

  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp ToVkStoreOp(GPUStoreOp op) {
  switch (op) {
    case GPUStoreOp::kStore:
      return VK_ATTACHMENT_STORE_OP_STORE;
    case GPUStoreOp::kDiscard:
      return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }

  return VK_ATTACHMENT_STORE_OP_STORE;
}

bool TransitionImageLayout(const VulkanContextState& state,
                           VkCommandBuffer command_buffer,
                           GPUTextureVK& texture, VkImageLayout new_layout) {
  const VkImageLayout old_layout = texture.GetCurrentLayout();
  if (old_layout == new_layout) {
    return true;
  }

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = texture.GetImage();
  barrier.subresourceRange.aspectMask =
      GetImageAspectMask(texture.GetDescriptor().format);
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount =
      texture.GetDescriptor().mip_level_count;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = AccessMaskForLayout(old_layout);
  barrier.dstAccessMask = AccessMaskForLayout(new_layout);

  state.DeviceFns().vkCmdPipelineBarrier(
      command_buffer, StageMaskForLayout(old_layout),
      StageMaskForLayout(new_layout), 0, 0, nullptr, 0, nullptr, 1, &barrier);
  texture.SetCurrentLayout(new_layout);
  return true;
}

struct AttachmentContext {
  GPUTextureVK* texture = nullptr;
  GPUTextureVK* resolve_texture = nullptr;
  VkImageLayout attachment_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageLayout final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

uint32_t GetRenderPassTargetWidth(const GPURenderPassDescriptor& desc) {
  if (desc.color_attachment.texture != nullptr) {
    return desc.color_attachment.texture->GetDescriptor().width;
  }
  if (desc.depth_attachment.texture != nullptr) {
    return desc.depth_attachment.texture->GetDescriptor().width;
  }
  if (desc.stencil_attachment.texture != nullptr) {
    return desc.stencil_attachment.texture->GetDescriptor().width;
  }
  return 0;
}

uint32_t GetRenderPassTargetHeight(const GPURenderPassDescriptor& desc) {
  if (desc.color_attachment.texture != nullptr) {
    return desc.color_attachment.texture->GetDescriptor().height;
  }
  if (desc.depth_attachment.texture != nullptr) {
    return desc.depth_attachment.texture->GetDescriptor().height;
  }
  if (desc.stencil_attachment.texture != nullptr) {
    return desc.stencil_attachment.texture->GetDescriptor().height;
  }
  return 0;
}

bool PrepareAttachmentContext(const GPUAttachment& attachment,
                              VkImageLayout attachment_layout,
                              AttachmentContext* context,
                              bool require_depth_stencil_format = false) {
  if (context == nullptr || attachment.texture == nullptr) {
    return false;
  }

  auto* texture = GPUTextureVK::Cast(attachment.texture.get());
  if (texture == nullptr || !texture->IsValid()) {
    return false;
  }
  if (require_depth_stencil_format &&
      !IsDepthStencilFormat(texture->GetDescriptor().format)) {
    return false;
  }

  context->texture = texture;
  context->attachment_layout = attachment_layout;
  context->final_layout = texture->GetPreferredLayout();

  if (attachment.resolve_texture != nullptr) {
    auto* resolve_texture = GPUTextureVK::Cast(attachment.resolve_texture.get());
    if (resolve_texture == nullptr || !resolve_texture->IsValid()) {
      return false;
    }
    context->resolve_texture = resolve_texture;
  }

  return true;
}

void LogUnsupportedCommandsIfNeeded(const GPURenderPass& pass,
                                    const std::string& label) {
  if (pass.GetCommands().empty()) {
    return;
  }

  if (label.empty()) {
    LOGW("GPURenderPassVK draw command encoding is not implemented yet; "
         "render pass will only begin/end");
    return;
  }

  LOGW("GPURenderPassVK draw command encoding is not implemented yet; pass "
       "'{}' will only begin/end", label);
}

bool RecordDynamicRenderingPass(const VulkanContextState& state,
                                GPUCommandBufferVK& command_buffer,
                                const GPURenderPass& pass) {
  const auto& desc = pass.GetDescriptor();
  if (desc.color_attachment.texture == nullptr &&
      desc.depth_attachment.texture == nullptr &&
      desc.stencil_attachment.texture == nullptr) {
    LOGE("Vulkan dynamic rendering requires at least one attachment");
    return false;
  }

  AttachmentContext color_context = {};
  if (desc.color_attachment.texture != nullptr &&
      !PrepareAttachmentContext(desc.color_attachment,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                &color_context)) {
    LOGE("Failed to prepare Vulkan color attachment");
    return false;
  }

  AttachmentContext depth_context = {};
  if (desc.depth_attachment.texture != nullptr &&
      !PrepareAttachmentContext(desc.depth_attachment,
                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                &depth_context, true)) {
    LOGE("Failed to prepare Vulkan depth attachment");
    return false;
  }

  AttachmentContext stencil_context = {};
  if (desc.stencil_attachment.texture != nullptr &&
      !PrepareAttachmentContext(
          desc.stencil_attachment,
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &stencil_context,
          true)) {
    LOGE("Failed to prepare Vulkan stencil attachment");
    return false;
  }

  if (color_context.texture != nullptr &&
      !TransitionImageLayout(state, command_buffer.GetCommandBuffer(),
                             *color_context.texture,
                             color_context.attachment_layout)) {
    return false;
  }

  VkRenderingAttachmentInfo color_info = {};
  if (color_context.texture != nullptr) {
    color_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_info.imageView = color_context.texture->GetImageView();
    color_info.imageLayout = color_context.attachment_layout;
    color_info.loadOp = ToVkLoadOp(desc.color_attachment.load_op);
    color_info.storeOp = ToVkStoreOp(desc.color_attachment.store_op);
    color_info.clearValue.color = {
        {static_cast<float>(desc.color_attachment.clear_value.r),
         static_cast<float>(desc.color_attachment.clear_value.g),
         static_cast<float>(desc.color_attachment.clear_value.b),
         static_cast<float>(desc.color_attachment.clear_value.a)}};
  }

  if (color_context.resolve_texture != nullptr) {
    if (!TransitionImageLayout(state, command_buffer.GetCommandBuffer(),
                               *color_context.resolve_texture,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)) {
      return false;
    }

    color_info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    color_info.resolveImageView = color_context.resolve_texture->GetImageView();
    color_info.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  if (depth_context.texture != nullptr &&
      !TransitionImageLayout(state, command_buffer.GetCommandBuffer(),
                             *depth_context.texture,
                             depth_context.attachment_layout)) {
    return false;
  }

  if (stencil_context.texture != nullptr &&
      stencil_context.texture != depth_context.texture &&
      !TransitionImageLayout(state, command_buffer.GetCommandBuffer(),
                             *stencil_context.texture,
                             stencil_context.attachment_layout)) {
    return false;
  }

  VkRenderingAttachmentInfo depth_info = {};
  if (depth_context.texture != nullptr) {
    depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_info.imageView = depth_context.texture->GetImageView();
    depth_info.imageLayout = depth_context.attachment_layout;
    depth_info.loadOp = ToVkLoadOp(desc.depth_attachment.load_op);
    depth_info.storeOp = ToVkStoreOp(desc.depth_attachment.store_op);
    depth_info.clearValue.depthStencil.depth = desc.depth_attachment.clear_value;
    depth_info.clearValue.depthStencil.stencil =
        desc.stencil_attachment.clear_value;
  }

  VkRenderingAttachmentInfo stencil_info = {};
  if (stencil_context.texture != nullptr) {
    stencil_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    stencil_info.imageView = stencil_context.texture->GetImageView();
    stencil_info.imageLayout = stencil_context.attachment_layout;
    stencil_info.loadOp = ToVkLoadOp(desc.stencil_attachment.load_op);
    stencil_info.storeOp = ToVkStoreOp(desc.stencil_attachment.store_op);
    stencil_info.clearValue.depthStencil.depth = desc.depth_attachment.clear_value;
    stencil_info.clearValue.depthStencil.stencil =
        desc.stencil_attachment.clear_value;
  }

  VkRenderingInfo rendering_info = {};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea.offset = {0, 0};
  rendering_info.renderArea.extent = {GetRenderPassTargetWidth(desc),
                                      GetRenderPassTargetHeight(desc)};
  rendering_info.layerCount = 1;
  rendering_info.colorAttachmentCount = color_context.texture != nullptr ? 1 : 0;
  rendering_info.pColorAttachments =
      color_context.texture != nullptr ? &color_info : nullptr;
  rendering_info.pDepthAttachment =
      depth_context.texture != nullptr ? &depth_info : nullptr;
  rendering_info.pStencilAttachment =
      stencil_context.texture != nullptr ? &stencil_info : nullptr;

  state.DeviceFns().vkCmdBeginRendering(command_buffer.GetCommandBuffer(),
                                        &rendering_info);
  LogUnsupportedCommandsIfNeeded(pass, desc.label);
  state.DeviceFns().vkCmdEndRendering(command_buffer.GetCommandBuffer());

  if (color_context.texture != nullptr) {
    TransitionImageLayout(state, command_buffer.GetCommandBuffer(),
                          *color_context.texture, color_context.final_layout);
  }
  if (color_context.resolve_texture != nullptr) {
    TransitionImageLayout(state, command_buffer.GetCommandBuffer(),
                          *color_context.resolve_texture,
                          color_context.resolve_texture->GetPreferredLayout());
  }
  if (depth_context.texture != nullptr) {
    TransitionImageLayout(state, command_buffer.GetCommandBuffer(),
                          *depth_context.texture, depth_context.final_layout);
  }
  if (stencil_context.texture != nullptr &&
      stencil_context.texture != depth_context.texture) {
    TransitionImageLayout(state, command_buffer.GetCommandBuffer(),
                          *stencil_context.texture,
                          stencil_context.final_layout);
  }

  const auto color_texture_ref = desc.color_attachment.texture;
  const auto resolve_texture_ref = desc.color_attachment.resolve_texture;
  const auto depth_texture_ref = desc.depth_attachment.texture;
  const auto stencil_texture_ref = desc.stencil_attachment.texture;
  command_buffer.RecordCleanupAction(
      [color_texture_ref, resolve_texture_ref, depth_texture_ref,
       stencil_texture_ref]() {});

  return true;
}

bool RecordLegacyRenderPass(const std::shared_ptr<const VulkanContextState>& state,
                            GPUCommandBufferVK& command_buffer,
                            const GPURenderPass& pass) {
  const auto& desc = pass.GetDescriptor();
  if (desc.color_attachment.texture == nullptr &&
      desc.depth_attachment.texture == nullptr &&
      desc.stencil_attachment.texture == nullptr) {
    LOGE("Vulkan legacy render pass requires at least one attachment");
    return false;
  }

  AttachmentContext color_context = {};
  if (desc.color_attachment.texture != nullptr &&
      !PrepareAttachmentContext(desc.color_attachment,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                &color_context)) {
    LOGE("Failed to prepare Vulkan legacy color attachment");
    return false;
  }

  AttachmentContext depth_context = {};
  if (desc.depth_attachment.texture != nullptr &&
      !PrepareAttachmentContext(desc.depth_attachment,
                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                &depth_context, true)) {
    LOGE("Failed to prepare Vulkan legacy depth attachment");
    return false;
  }

  AttachmentContext stencil_context = {};
  if (desc.stencil_attachment.texture != nullptr &&
      !PrepareAttachmentContext(
          desc.stencil_attachment,
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &stencil_context,
          true)) {
    LOGE("Failed to prepare Vulkan legacy stencil attachment");
    return false;
  }

  const bool has_combined_depth_stencil =
      depth_context.texture != nullptr && stencil_context.texture != nullptr &&
      depth_context.texture == stencil_context.texture;
  if (depth_context.texture != nullptr && stencil_context.texture != nullptr &&
      depth_context.texture != stencil_context.texture) {
    LOGE("Vulkan legacy render pass does not support separate depth and "
         "stencil textures yet");
    return false;
  }

  std::array<VkAttachmentDescription, 3> attachment_descs = {};
  std::array<VkImageView, 3> attachments = {};
  std::array<VkClearValue, 3> clear_values = {};
  uint32_t attachment_count = 0;
  uint32_t clear_value_count = 0;

  VkAttachmentReference color_ref = {};
  const VkAttachmentReference* color_ref_ptr = nullptr;
  if (color_context.texture != nullptr) {
    attachment_descs[attachment_count].format =
        ToVkFormat(color_context.texture->GetDescriptor().format);
    attachment_descs[attachment_count].samples =
        static_cast<VkSampleCountFlagBits>(
            color_context.texture->GetDescriptor().sample_count);
    attachment_descs[attachment_count].loadOp =
        ToVkLoadOp(desc.color_attachment.load_op);
    attachment_descs[attachment_count].storeOp =
        ToVkStoreOp(desc.color_attachment.store_op);
    attachment_descs[attachment_count].stencilLoadOp =
        VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[attachment_count].stencilStoreOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[attachment_count].initialLayout =
        color_context.texture->GetCurrentLayout();
    attachment_descs[attachment_count].finalLayout = color_context.final_layout;
    attachments[attachment_count] = color_context.texture->GetImageView();
    clear_values[clear_value_count].color = {
        {static_cast<float>(desc.color_attachment.clear_value.r),
         static_cast<float>(desc.color_attachment.clear_value.g),
         static_cast<float>(desc.color_attachment.clear_value.b),
         static_cast<float>(desc.color_attachment.clear_value.a)}};

    color_ref.attachment = attachment_count;
    color_ref.layout = color_context.attachment_layout;
    color_ref_ptr = &color_ref;
    ++attachment_count;
    ++clear_value_count;
  }

  VkAttachmentReference resolve_ref = {};
  const VkAttachmentReference* resolve_ref_ptr = nullptr;
  if (color_context.resolve_texture != nullptr) {
    attachment_descs[attachment_count].format =
        ToVkFormat(color_context.resolve_texture->GetDescriptor().format);
    attachment_descs[attachment_count].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[attachment_count].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[attachment_count].storeOp =
        ToVkStoreOp(desc.color_attachment.store_op);
    attachment_descs[attachment_count].stencilLoadOp =
        VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[attachment_count].stencilStoreOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[attachment_count].initialLayout =
        color_context.resolve_texture->GetCurrentLayout();
    attachment_descs[attachment_count].finalLayout =
        color_context.resolve_texture->GetPreferredLayout();
    attachments[attachment_count] = color_context.resolve_texture->GetImageView();
    resolve_ref.attachment = attachment_count;
    resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    resolve_ref_ptr = &resolve_ref;
    ++attachment_count;
    ++clear_value_count;
  }

  VkAttachmentReference depth_stencil_ref = {};
  const VkAttachmentReference* depth_stencil_ref_ptr = nullptr;
  if (depth_context.texture != nullptr || stencil_context.texture != nullptr) {
    auto* depth_stencil_texture =
        depth_context.texture != nullptr ? depth_context.texture
                                         : stencil_context.texture;
    attachment_descs[attachment_count].format =
        ToVkFormat(depth_stencil_texture->GetDescriptor().format);
    attachment_descs[attachment_count].samples =
        static_cast<VkSampleCountFlagBits>(
            depth_stencil_texture->GetDescriptor().sample_count);
    attachment_descs[attachment_count].loadOp =
        depth_context.texture != nullptr ? ToVkLoadOp(desc.depth_attachment.load_op)
                                         : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[attachment_count].storeOp =
        depth_context.texture != nullptr
            ? ToVkStoreOp(desc.depth_attachment.store_op)
            : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[attachment_count].stencilLoadOp =
        stencil_context.texture != nullptr
            ? ToVkLoadOp(desc.stencil_attachment.load_op)
            : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[attachment_count].stencilStoreOp =
        stencil_context.texture != nullptr
            ? ToVkStoreOp(desc.stencil_attachment.store_op)
            : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[attachment_count].initialLayout =
        depth_stencil_texture->GetCurrentLayout();
    attachment_descs[attachment_count].finalLayout =
        depth_stencil_texture->GetPreferredLayout();
    attachments[attachment_count] = depth_stencil_texture->GetImageView();
    clear_values[clear_value_count].depthStencil.depth =
        desc.depth_attachment.clear_value;
    clear_values[clear_value_count].depthStencil.stencil =
        desc.stencil_attachment.clear_value;

    depth_stencil_ref.attachment = attachment_count;
    depth_stencil_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_stencil_ref_ptr = &depth_stencil_ref;
    ++attachment_count;
    ++clear_value_count;
  }

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = color_ref_ptr != nullptr ? 1 : 0;
  subpass.pColorAttachments = color_ref_ptr;
  subpass.pResolveAttachments = resolve_ref_ptr;
  subpass.pDepthStencilAttachment = depth_stencil_ref_ptr;

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = attachment_count;
  render_pass_info.pAttachments = attachment_descs.data();
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;

  VkRenderPass render_pass = VK_NULL_HANDLE;
  VkResult result = state->DeviceFns().vkCreateRenderPass(
      state->GetLogicalDevice(), &render_pass_info, nullptr, &render_pass);
  if (result != VK_SUCCESS || render_pass == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan render pass: {}",
         static_cast<int32_t>(result));
    return false;
  }

  VkFramebufferCreateInfo framebuffer_info = {};
  framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_info.renderPass = render_pass;
  framebuffer_info.attachmentCount = attachment_count;
  framebuffer_info.pAttachments = attachments.data();
  framebuffer_info.width = GetRenderPassTargetWidth(desc);
  framebuffer_info.height = GetRenderPassTargetHeight(desc);
  framebuffer_info.layers = 1;

  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  result = state->DeviceFns().vkCreateFramebuffer(
      state->GetLogicalDevice(), &framebuffer_info, nullptr, &framebuffer);
  if (result != VK_SUCCESS || framebuffer == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan framebuffer: {}",
         static_cast<int32_t>(result));
    state->DeviceFns().vkDestroyRenderPass(state->GetLogicalDevice(),
                                           render_pass, nullptr);
    return false;
  }

  VkRenderPassBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin_info.renderPass = render_pass;
  begin_info.framebuffer = framebuffer;
  begin_info.renderArea.offset = {0, 0};
  begin_info.renderArea.extent = {GetRenderPassTargetWidth(desc),
                                  GetRenderPassTargetHeight(desc)};
  begin_info.clearValueCount = clear_value_count;
  begin_info.pClearValues = clear_values.data();

  state->DeviceFns().vkCmdBeginRenderPass(command_buffer.GetCommandBuffer(),
                                          &begin_info,
                                          VK_SUBPASS_CONTENTS_INLINE);
  LogUnsupportedCommandsIfNeeded(pass, desc.label);
  state->DeviceFns().vkCmdEndRenderPass(command_buffer.GetCommandBuffer());

  if (color_context.texture != nullptr) {
    color_context.texture->SetCurrentLayout(color_context.final_layout);
  }
  if (color_context.resolve_texture != nullptr) {
    color_context.resolve_texture->SetCurrentLayout(
        color_context.resolve_texture->GetPreferredLayout());
  }
  if (depth_context.texture != nullptr) {
    depth_context.texture->SetCurrentLayout(depth_context.final_layout);
  }
  if (stencil_context.texture != nullptr &&
      (!has_combined_depth_stencil ||
       stencil_context.texture != depth_context.texture)) {
    stencil_context.texture->SetCurrentLayout(stencil_context.final_layout);
  }

  command_buffer.RecordCleanupAction([state, framebuffer, render_pass]() {
    if (state == nullptr || state->GetLogicalDevice() == VK_NULL_HANDLE) {
      return;
    }

    if (framebuffer != VK_NULL_HANDLE &&
        state->DeviceFns().vkDestroyFramebuffer != nullptr) {
      state->DeviceFns().vkDestroyFramebuffer(state->GetLogicalDevice(),
                                              framebuffer, nullptr);
    }

    if (render_pass != VK_NULL_HANDLE &&
        state->DeviceFns().vkDestroyRenderPass != nullptr) {
      state->DeviceFns().vkDestroyRenderPass(state->GetLogicalDevice(),
                                             render_pass, nullptr);
    }
  });
  const auto color_texture_ref = desc.color_attachment.texture;
  const auto resolve_texture_ref = desc.color_attachment.resolve_texture;
  const auto depth_texture_ref = desc.depth_attachment.texture;
  const auto stencil_texture_ref = desc.stencil_attachment.texture;
  command_buffer.RecordCleanupAction(
      [color_texture_ref, resolve_texture_ref, depth_texture_ref,
       stencil_texture_ref]() {});

  return true;
}

}  // namespace

GPURenderPassVK::GPURenderPassVK(std::shared_ptr<const VulkanContextState> state,
                                 GPUCommandBufferVK* command_buffer,
                                 const GPURenderPassDescriptor& desc)
    : GPURenderPass(desc),
      state_(std::move(state)),
      command_buffer_(command_buffer) {}

void GPURenderPassVK::EncodeCommands(std::optional<GPUViewport> viewport,
                                     std::optional<GPUScissorRect> scissor) {
  (void)viewport;
  (void)scissor;

  if (state_ == nullptr || command_buffer_ == nullptr) {
    LOGE("Failed to encode Vulkan render pass: invalid state");
    return;
  }

  if (state_->IsDynamicRenderingEnabled()) {
    RecordDynamicRenderingPass(*state_, *command_buffer_, *this);
    return;
  }

  RecordLegacyRenderPass(state_, *command_buffer_, *this);
}

}  // namespace skity
