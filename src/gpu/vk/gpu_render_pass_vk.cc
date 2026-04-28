// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_render_pass_vk.hpp"

#include <array>

#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/gpu_render_pipeline_vk.hpp"
#include "src/gpu/vk/gpu_sampler_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/gpu/vk/vulkan_context_state.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

bool IsDepthStencilFormat(GPUTextureFormat format) {
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

bool HasTextureUsage(const GPUTextureDescriptor& descriptor,
                     GPUTextureUsage usage) {
  return (descriptor.usage & static_cast<GPUTextureUsageMask>(usage)) != 0;
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

VkAttachmentStoreOp ToColorAttachmentStoreOp(bool has_resolve, GPUStoreOp op) {
  if (has_resolve) {
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }
  return ToVkStoreOp(op);
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
  barrier.subresourceRange.levelCount = texture.GetDescriptor().mip_level_count;
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
    auto* resolve_texture =
        GPUTextureVK::Cast(attachment.resolve_texture.get());
    if (resolve_texture == nullptr || !resolve_texture->IsValid()) {
      return false;
    }
    if (!HasTextureUsage(resolve_texture->GetDescriptor(),
                         GPUTextureUsage::kRenderAttachment)) {
      LOGE("Vulkan resolve texture must use RenderAttachment usage");
      return false;
    }
    context->resolve_texture = resolve_texture;
  }

  return true;
}

bool PrepareColorAttachment(const GPURenderPassDescriptor& desc,
                            AttachmentContext* color_context) {
  if (!PrepareAttachmentContext(desc.color_attachment,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                color_context)) {
    LOGE("Failed to prepare Vulkan color attachment");
    return false;
  }

  return true;
}

bool PrepareDepthStencilAttachment(const GPURenderPassDescriptor& desc,
                                   AttachmentContext* depth_stencil_context,
                                   bool* has_depth, bool* has_stencil) {
  if (depth_stencil_context == nullptr || has_depth == nullptr ||
      has_stencil == nullptr) {
    return false;
  }

  *has_depth = desc.depth_attachment.texture != nullptr;
  *has_stencil = desc.stencil_attachment.texture != nullptr;

  if (!*has_depth && !*has_stencil) {
    return true;
  }

  if (!*has_stencil) {
    LOGE("Vulkan render pass requires stencil when depth is attached");
    return false;
  }

  if (*has_depth &&
      desc.depth_attachment.texture != desc.stencil_attachment.texture) {
    LOGE("Vulkan render pass requires depth and stencil to share one texture");
    return false;
  }

  if (!PrepareAttachmentContext(
          desc.stencil_attachment,
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          depth_stencil_context, true)) {
    LOGE("Failed to prepare Vulkan depth/stencil attachment");
    return false;
  }

  const auto format = depth_stencil_context->texture->GetDescriptor().format;
  if (*has_depth && format != GPUTextureFormat::kDepth24Stencil8) {
    LOGE("Vulkan depth attachment must use Depth24Stencil8 format");
    return false;
  }

  if (!*has_depth && format != GPUTextureFormat::kStencil8) {
    LOGE("Vulkan stencil-only attachment must use Stencil8 format");
    return false;
  }

  return true;
}

VulkanContextState::LegacyRenderPassKey BuildLegacyRenderPassKey(
    const GPURenderPassDescriptor& desc, const AttachmentContext& color_context,
    const AttachmentContext& depth_stencil_context, bool has_depth,
    bool has_stencil) {
  VulkanContextState::LegacyRenderPassKey key = {};
  key.color_format = color_context.texture->GetVkFormat();
  key.color_samples = static_cast<VkSampleCountFlagBits>(
      color_context.texture->GetDescriptor().sample_count);
  key.color_load_op = ToVkLoadOp(desc.color_attachment.load_op);
  key.color_store_op = ToColorAttachmentStoreOp(
      color_context.resolve_texture != nullptr, desc.color_attachment.store_op);
  key.has_resolve = color_context.resolve_texture != nullptr;

  if (key.has_resolve) {
    key.resolve_format = color_context.resolve_texture->GetVkFormat();
    key.resolve_store_op = ToVkStoreOp(desc.color_attachment.store_op);
  }

  key.has_depth = has_depth;
  key.has_stencil = has_stencil;
  if (has_depth || has_stencil) {
    key.depth_stencil_format = depth_stencil_context.texture->GetVkFormat();
    key.depth_stencil_samples = static_cast<VkSampleCountFlagBits>(
        depth_stencil_context.texture->GetDescriptor().sample_count);
    key.depth_load_op = has_depth ? ToVkLoadOp(desc.depth_attachment.load_op)
                                  : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    key.depth_store_op = has_depth ? ToVkStoreOp(desc.depth_attachment.store_op)
                                   : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    key.stencil_load_op = ToVkLoadOp(desc.stencil_attachment.load_op);
    key.stencil_store_op = ToVkStoreOp(desc.stencil_attachment.store_op);
  }

  return key;
}

VkImageLayout GetSampledImageLayout(const GPUTextureVK& texture) {
  const auto current_layout = texture.GetCurrentLayout();
  return current_layout != VK_IMAGE_LAYOUT_UNDEFINED
             ? current_layout
             : texture.GetPreferredLayout();
}

struct DescriptorPoolRequirements {
  uint32_t max_sets = 0;
  uint32_t uniform_buffer_count = 0;
  uint32_t sampled_image_count = 0;
  uint32_t sampler_count = 0;
};

void AccumulateDescriptorPoolRequirements(const Command& command,
                                          const GPURenderPipelineVK& pipeline,
                                          DescriptorPoolRequirements* req) {
  if (req == nullptr) {
    return;
  }

  req->max_sets +=
      static_cast<uint32_t>(pipeline.GetDescriptorSetLayouts().size());
  req->uniform_buffer_count +=
      static_cast<uint32_t>(command.uniform_bindings.size());
  req->sampled_image_count +=
      static_cast<uint32_t>(command.texture_bindings.size());
  req->sampler_count += static_cast<uint32_t>(command.sampler_bindings.size());
}

VkDescriptorPool CreateDescriptorPoolForPass(
    const std::shared_ptr<const VulkanContextState>& state,
    const GPURenderPass& pass) {
  if (state == nullptr || state->GetLogicalDevice() == VK_NULL_HANDLE) {
    return VK_NULL_HANDLE;
  }

  DescriptorPoolRequirements requirements = {};
  for (const auto* command : pass.GetCommands()) {
    if (command == nullptr || !command->IsValid()) {
      continue;
    }

    auto* pipeline = GPURenderPipelineVK::Cast(command->pipeline);
    if (pipeline == nullptr || !pipeline->IsValid()) {
      continue;
    }

    AccumulateDescriptorPoolRequirements(*command, *pipeline, &requirements);
  }

  if (requirements.max_sets == 0) {
    return VK_NULL_HANDLE;
  }

  std::vector<VkDescriptorPoolSize> pool_sizes;
  if (requirements.uniform_buffer_count > 0) {
    pool_sizes.push_back(
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, requirements.uniform_buffer_count});
  }
  if (requirements.sampled_image_count > 0) {
    pool_sizes.push_back(
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, requirements.sampled_image_count});
  }
  if (requirements.sampler_count > 0) {
    pool_sizes.push_back(
        {VK_DESCRIPTOR_TYPE_SAMPLER, requirements.sampler_count});
  }

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = requirements.max_sets;
  pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.empty() ? nullptr : pool_sizes.data();

  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  const VkResult result = state->DeviceFns().vkCreateDescriptorPool(
      state->GetLogicalDevice(), &pool_info, nullptr, &descriptor_pool);
  if (result != VK_SUCCESS || descriptor_pool == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan descriptor pool: {}",
         static_cast<int32_t>(result));
    return VK_NULL_HANDLE;
  }

  return descriptor_pool;
}

bool PrepareSampledTextures(const VulkanContextState& state,
                            GPUCommandBufferVK& command_buffer,
                            const GPURenderPass& pass) {
  for (const auto* command : pass.GetCommands()) {
    if (command == nullptr || !command->IsValid()) {
      continue;
    }

    for (const auto& binding : command->texture_bindings) {
      auto* texture = GPUTextureVK::Cast(binding.texture.get());
      if (texture == nullptr || !texture->IsValid()) {
        LOGE("Failed to prepare Vulkan sampled texture binding");
        return false;
      }

      if (!TransitionImageLayout(state, command_buffer.GetCommandBuffer(),
                                 *texture, texture->GetPreferredLayout())) {
        return false;
      }
    }
  }

  return true;
}

void RecordBoundResourceCleanup(GPUCommandBufferVK& command_buffer,
                                const GPURenderPass& pass) {
  std::vector<std::shared_ptr<GPUTexture>> textures;
  std::vector<std::shared_ptr<GPUSampler>> samplers;

  for (const auto* command : pass.GetCommands()) {
    if (command == nullptr || !command->IsValid()) {
      continue;
    }

    for (const auto& binding : command->texture_bindings) {
      if (binding.texture != nullptr) {
        textures.emplace_back(binding.texture);
      }
    }

    for (const auto& binding : command->sampler_bindings) {
      if (binding.sampler != nullptr) {
        samplers.emplace_back(binding.sampler);
      }
    }
  }

  command_buffer.RecordCleanupAction(
      [textures = std::move(textures), samplers = std::move(samplers)]() {});
}

bool SetupDescriptorSets(const VulkanContextState& state,
                         VkCommandBuffer command_buffer,
                         VkDescriptorPool descriptor_pool,
                         const Command& command,
                         const GPURenderPipelineVK& pipeline) {
  const auto& set_layouts = pipeline.GetDescriptorSetLayouts();
  if (set_layouts.empty()) {
    return true;
  }

  if (descriptor_pool == VK_NULL_HANDLE) {
    LOGE("Failed to bind Vulkan descriptor sets: descriptor pool is null");
    return false;
  }

  std::vector<VkDescriptorSet> descriptor_sets(set_layouts.size(),
                                               VK_NULL_HANDLE);
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool;
  alloc_info.descriptorSetCount = static_cast<uint32_t>(set_layouts.size());
  alloc_info.pSetLayouts = set_layouts.data();

  const VkResult alloc_result = state.DeviceFns().vkAllocateDescriptorSets(
      state.GetLogicalDevice(), &alloc_info, descriptor_sets.data());
  if (alloc_result != VK_SUCCESS) {
    LOGE("Failed to allocate Vulkan descriptor sets: {}",
         static_cast<int32_t>(alloc_result));
    return false;
  }

  const size_t total_write_count = command.uniform_bindings.size() +
                                   command.texture_bindings.size() +
                                   command.sampler_bindings.size();
  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(total_write_count);
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  buffer_infos.reserve(command.uniform_bindings.size());
  std::vector<VkDescriptorImageInfo> image_infos;
  image_infos.reserve(command.texture_bindings.size() +
                      command.sampler_bindings.size());

  for (const auto& binding : command.uniform_bindings) {
    if (binding.group >= descriptor_sets.size()) {
      LOGE("Invalid Vulkan uniform binding group {}", binding.group);
      return false;
    }

    auto* buffer = static_cast<GPUBufferVK*>(binding.buffer.buffer);
    if (buffer == nullptr || !buffer->IsValid()) {
      LOGE("Invalid Vulkan uniform buffer binding");
      return false;
    }

    buffer_infos.push_back(VkDescriptorBufferInfo{
        buffer->GetBuffer(),
        binding.buffer.offset,
        binding.buffer.range,
    });

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_sets[binding.group];
    write.dstBinding = binding.binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &buffer_infos.back();
    writes.push_back(write);
  }

  for (const auto& binding : command.texture_bindings) {
    if (binding.group >= descriptor_sets.size()) {
      LOGE("Invalid Vulkan texture binding group {}", binding.group);
      return false;
    }

    auto* texture = GPUTextureVK::Cast(binding.texture.get());
    if (texture == nullptr || !texture->IsValid()) {
      LOGE("Invalid Vulkan texture binding");
      return false;
    }

    image_infos.push_back(VkDescriptorImageInfo{
        VK_NULL_HANDLE,
        texture->GetImageView(),
        GetSampledImageLayout(*texture),
    });

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_sets[binding.group];
    write.dstBinding = binding.binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &image_infos.back();
    writes.push_back(write);
  }

  for (const auto& binding : command.sampler_bindings) {
    if (binding.group >= descriptor_sets.size()) {
      LOGE("Invalid Vulkan sampler binding group {}", binding.group);
      return false;
    }

    auto* sampler = GPUSamplerVK::Cast(binding.sampler.get());
    if (sampler == nullptr || !sampler->IsValid()) {
      LOGE("Invalid Vulkan sampler binding");
      return false;
    }

    image_infos.push_back(VkDescriptorImageInfo{
        sampler->GetSampler(),
        VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_UNDEFINED,
    });

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_sets[binding.group];
    write.dstBinding = binding.binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    write.pImageInfo = &image_infos.back();
    writes.push_back(write);
  }

  if (!writes.empty()) {
    state.DeviceFns().vkUpdateDescriptorSets(
        state.GetLogicalDevice(), static_cast<uint32_t>(writes.size()),
        writes.data(), 0, nullptr);
  }

  state.DeviceFns().vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipeline.GetPipelineLayout(), 0,
      static_cast<uint32_t>(descriptor_sets.size()), descriptor_sets.data(), 0,
      nullptr);
  return true;
}

bool RecordDrawCommands(const std::shared_ptr<const VulkanContextState>& state,
                        GPUCommandBufferVK& command_buffer,
                        const GPURenderPass& pass,
                        std::optional<GPUViewport> viewport,
                        std::optional<GPUScissorRect> scissor) {
  const auto target_width = pass.GetDescriptor().GetTargetWidth();
  const auto target_height = pass.GetDescriptor().GetTargetHeight();

  const GPUViewport vp = viewport.value_or(GPUViewport{
      0.f,
      0.f,
      static_cast<float>(target_width),
      static_cast<float>(target_height),
      0.f,
      1.f,
  });
  const GPUScissorRect default_scissor = scissor.value_or(GPUScissorRect{
      0,
      0,
      target_width,
      target_height,
  });

  VkViewport vk_viewport = {};
  vk_viewport.x = vp.x;
  vk_viewport.y = vp.y + vp.height;
  vk_viewport.width = vp.width;
  vk_viewport.height = -1.f * vp.height;
  vk_viewport.minDepth = vp.min_depth;
  vk_viewport.maxDepth = vp.max_depth;
  state->DeviceFns().vkCmdSetViewport(command_buffer.GetCommandBuffer(), 0, 1,
                                      &vk_viewport);

  VkRect2D vk_scissor = {};
  vk_scissor.offset = {static_cast<int32_t>(default_scissor.x),
                       static_cast<int32_t>(default_scissor.y)};
  vk_scissor.extent = {default_scissor.width, default_scissor.height};
  state->DeviceFns().vkCmdSetScissor(command_buffer.GetCommandBuffer(), 0, 1,
                                     &vk_scissor);

  VkDescriptorPool descriptor_pool = CreateDescriptorPoolForPass(state, pass);
  if (descriptor_pool != VK_NULL_HANDLE) {
    std::weak_ptr<const VulkanContextState> weak_state = state;
    command_buffer.RecordCleanupAction([weak_state, descriptor_pool]() {
      auto state = weak_state.lock();
      if (state == nullptr || state->GetLogicalDevice() == VK_NULL_HANDLE ||
          state->DeviceFns().vkDestroyDescriptorPool == nullptr) {
        return;
      }
      state->DeviceFns().vkDestroyDescriptorPool(state->GetLogicalDevice(),
                                                 descriptor_pool, nullptr);
    });
  }

  for (const auto* command : pass.GetCommands()) {
    if (command == nullptr || !command->IsValid()) {
      continue;
    }

    auto* pipeline = GPURenderPipelineVK::Cast(command->pipeline);
    if (pipeline == nullptr || !pipeline->IsValid()) {
      LOGE("Failed to record Vulkan draw command: invalid pipeline");
      return false;
    }

    state->DeviceFns().vkCmdBindPipeline(command_buffer.GetCommandBuffer(),
                                         VK_PIPELINE_BIND_POINT_GRAPHICS,
                                         pipeline->GetPipeline());

    VkRect2D command_scissor = {};
    command_scissor.offset = {static_cast<int32_t>(command->scissor_rect.x),
                              static_cast<int32_t>(command->scissor_rect.y)};
    command_scissor.extent = {command->scissor_rect.width,
                              command->scissor_rect.height};
    state->DeviceFns().vkCmdSetScissor(command_buffer.GetCommandBuffer(), 0, 1,
                                       &command_scissor);

    auto* vertex_buffer =
        static_cast<GPUBufferVK*>(command->vertex_buffer.buffer);
    auto* index_buffer =
        static_cast<GPUBufferVK*>(command->index_buffer.buffer);
    if (vertex_buffer == nullptr || !vertex_buffer->IsValid() ||
        index_buffer == nullptr || !index_buffer->IsValid()) {
      LOGE("Failed to record Vulkan draw command: invalid vertex/index buffer");
      return false;
    }

    std::array<VkBuffer, 2> vertex_buffers = {vertex_buffer->GetBuffer(),
                                              VK_NULL_HANDLE};
    std::array<VkDeviceSize, 2> vertex_offsets = {
        command->vertex_buffer.offset,
        0,
    };
    uint32_t vertex_buffer_count = 1;
    if (command->IsInstanced()) {
      auto* instance_buffer =
          static_cast<GPUBufferVK*>(command->instance_buffer.buffer);
      if (instance_buffer == nullptr || !instance_buffer->IsValid()) {
        LOGE("Failed to record Vulkan draw command: invalid instance buffer");
        return false;
      }
      vertex_buffers[1] = instance_buffer->GetBuffer();
      vertex_offsets[1] = command->instance_buffer.offset;
      vertex_buffer_count = 2;
    }

    state->DeviceFns().vkCmdBindVertexBuffers(
        command_buffer.GetCommandBuffer(), 0, vertex_buffer_count,
        vertex_buffers.data(), vertex_offsets.data());
    state->DeviceFns().vkCmdBindIndexBuffer(
        command_buffer.GetCommandBuffer(), index_buffer->GetBuffer(),
        command->index_buffer.offset, VK_INDEX_TYPE_UINT32);

    if (!SetupDescriptorSets(*state, command_buffer.GetCommandBuffer(),
                             descriptor_pool, *command, *pipeline)) {
      return false;
    }

    state->DeviceFns().vkCmdSetStencilCompareMask(
        command_buffer.GetCommandBuffer(), VK_STENCIL_FACE_FRONT_BIT,
        command->front_stencil_compare_mask);
    state->DeviceFns().vkCmdSetStencilCompareMask(
        command_buffer.GetCommandBuffer(), VK_STENCIL_FACE_BACK_BIT,
        command->back_stencil_compare_mask);
    state->DeviceFns().vkCmdSetStencilWriteMask(
        command_buffer.GetCommandBuffer(), VK_STENCIL_FACE_FRONT_BIT,
        command->front_stencil_write_mask);
    state->DeviceFns().vkCmdSetStencilWriteMask(
        command_buffer.GetCommandBuffer(), VK_STENCIL_FACE_BACK_BIT,
        command->back_stencil_write_mask);
    state->DeviceFns().vkCmdSetStencilReference(
        command_buffer.GetCommandBuffer(),
        VK_STENCIL_FACE_FRONT_BIT | VK_STENCIL_FACE_BACK_BIT,
        command->stencil_reference);

    state->DeviceFns().vkCmdDrawIndexed(
        command_buffer.GetCommandBuffer(), command->index_count,
        command->instance_count == 0 ? 1 : command->instance_count, 0, 0, 0);
  }

  RecordBoundResourceCleanup(command_buffer, pass);
  return true;
}

bool RecordDynamicRenderingPass(
    const std::shared_ptr<const VulkanContextState>& state,
    GPUCommandBufferVK& command_buffer, const GPURenderPass& pass,
    std::optional<GPUViewport> viewport,
    std::optional<GPUScissorRect> scissor) {
  const auto& desc = pass.GetDescriptor();
  if (desc.color_attachment.texture == nullptr) {
    LOGE("Vulkan dynamic rendering requires a color attachment");
    return false;
  }

  AttachmentContext color_context = {};
  if (!PrepareColorAttachment(desc, &color_context)) {
    return false;
  }

  AttachmentContext depth_stencil_context = {};
  bool has_depth = false;
  bool has_stencil = false;
  if (!PrepareDepthStencilAttachment(desc, &depth_stencil_context, &has_depth,
                                     &has_stencil)) {
    return false;
  }

  if (!PrepareSampledTextures(*state, command_buffer, pass)) {
    return false;
  }

  if (!TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                             *color_context.texture,
                             color_context.attachment_layout)) {
    return false;
  }

  VkRenderingAttachmentInfo color_info = {};
  color_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_info.imageView = color_context.texture->GetImageView();
  color_info.imageLayout = color_context.attachment_layout;
  color_info.loadOp = ToVkLoadOp(desc.color_attachment.load_op);
  color_info.storeOp = ToColorAttachmentStoreOp(
      color_context.resolve_texture != nullptr, desc.color_attachment.store_op);
  color_info.clearValue.color = {
      {static_cast<float>(desc.color_attachment.clear_value.r),
       static_cast<float>(desc.color_attachment.clear_value.g),
       static_cast<float>(desc.color_attachment.clear_value.b),
       static_cast<float>(desc.color_attachment.clear_value.a)}};

  if (color_context.resolve_texture != nullptr) {
    if (!TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                               *color_context.resolve_texture,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)) {
      return false;
    }

    color_info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    color_info.resolveImageView = color_context.resolve_texture->GetImageView();
    color_info.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  if (depth_stencil_context.texture != nullptr &&
      !TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                             *depth_stencil_context.texture,
                             depth_stencil_context.attachment_layout)) {
    return false;
  }

  VkRenderingAttachmentInfo depth_info = {};
  if (has_depth) {
    depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_info.imageView = depth_stencil_context.texture->GetImageView();
    depth_info.imageLayout = depth_stencil_context.attachment_layout;
    depth_info.loadOp = ToVkLoadOp(desc.depth_attachment.load_op);
    depth_info.storeOp = ToVkStoreOp(desc.depth_attachment.store_op);
    depth_info.clearValue.depthStencil.depth =
        desc.depth_attachment.clear_value;
    depth_info.clearValue.depthStencil.stencil =
        desc.stencil_attachment.clear_value;
  }

  VkRenderingAttachmentInfo stencil_info = {};
  if (has_stencil) {
    stencil_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    stencil_info.imageView = depth_stencil_context.texture->GetImageView();
    stencil_info.imageLayout = depth_stencil_context.attachment_layout;
    stencil_info.loadOp = ToVkLoadOp(desc.stencil_attachment.load_op);
    stencil_info.storeOp = ToVkStoreOp(desc.stencil_attachment.store_op);
    stencil_info.clearValue.depthStencil.depth =
        desc.depth_attachment.clear_value;
    stencil_info.clearValue.depthStencil.stencil =
        desc.stencil_attachment.clear_value;
  }

  VkRenderingInfo rendering_info = {};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea.offset = {0, 0};
  rendering_info.renderArea.extent = {desc.GetTargetWidth(),
                                      desc.GetTargetHeight()};
  rendering_info.layerCount = 1;
  rendering_info.colorAttachmentCount = 1;
  rendering_info.pColorAttachments = &color_info;
  rendering_info.pDepthAttachment = has_depth ? &depth_info : nullptr;
  rendering_info.pStencilAttachment = has_stencil ? &stencil_info : nullptr;

  state->DeviceFns().vkCmdBeginRendering(command_buffer.GetCommandBuffer(),
                                         &rendering_info);
  const bool recorded =
      RecordDrawCommands(state, command_buffer, pass, viewport, scissor);
  state->DeviceFns().vkCmdEndRendering(command_buffer.GetCommandBuffer());
  if (!recorded) {
    return false;
  }

  TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                        *color_context.texture, color_context.final_layout);
  if (color_context.resolve_texture != nullptr) {
    TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                          *color_context.resolve_texture,
                          color_context.resolve_texture->GetPreferredLayout());
  }
  if (depth_stencil_context.texture != nullptr) {
    TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                          *depth_stencil_context.texture,
                          depth_stencil_context.final_layout);
  }

  const auto color_texture_ref = desc.color_attachment.texture;
  const auto resolve_texture_ref = desc.color_attachment.resolve_texture;
  const auto depth_texture_ref = desc.depth_attachment.texture;
  const auto stencil_texture_ref = desc.stencil_attachment.texture;
  command_buffer.RecordCleanupAction([color_texture_ref, resolve_texture_ref,
                                      depth_texture_ref,
                                      stencil_texture_ref]() {});

  return true;
}

bool RecordLegacyRenderPass(
    const std::shared_ptr<const VulkanContextState>& state,
    GPUCommandBufferVK& command_buffer, const GPURenderPass& pass,
    std::optional<GPUViewport> viewport,
    std::optional<GPUScissorRect> scissor) {
  const auto& desc = pass.GetDescriptor();
  if (desc.color_attachment.texture == nullptr) {
    LOGE("Vulkan legacy render pass requires a color attachment");
    return false;
  }

  AttachmentContext color_context = {};
  if (!PrepareColorAttachment(desc, &color_context)) {
    return false;
  }

  AttachmentContext depth_stencil_context = {};
  bool has_depth = false;
  bool has_stencil = false;
  if (!PrepareDepthStencilAttachment(desc, &depth_stencil_context, &has_depth,
                                     &has_stencil)) {
    return false;
  }

  if (!PrepareSampledTextures(*state, command_buffer, pass)) {
    return false;
  }

  if (!TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                             *color_context.texture,
                             color_context.attachment_layout)) {
    return false;
  }
  if (color_context.resolve_texture != nullptr &&
      !TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                             *color_context.resolve_texture,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)) {
    return false;
  }
  if (depth_stencil_context.texture != nullptr &&
      !TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                             *depth_stencil_context.texture,
                             depth_stencil_context.attachment_layout)) {
    return false;
  }

  std::array<VkImageView, 3> attachments = {};
  std::array<VkClearValue, 3> clear_values = {};
  uint32_t attachment_count = 0;
  uint32_t clear_value_count = 0;
  attachments[attachment_count] = color_context.texture->GetImageView();
  clear_values[clear_value_count].color = {
      {static_cast<float>(desc.color_attachment.clear_value.r),
       static_cast<float>(desc.color_attachment.clear_value.g),
       static_cast<float>(desc.color_attachment.clear_value.b),
       static_cast<float>(desc.color_attachment.clear_value.a)}};
  ++attachment_count;
  ++clear_value_count;

  if (color_context.resolve_texture != nullptr) {
    attachments[attachment_count] =
        color_context.resolve_texture->GetImageView();
    ++attachment_count;
    ++clear_value_count;
  }

  if (depth_stencil_context.texture != nullptr) {
    attachments[attachment_count] =
        depth_stencil_context.texture->GetImageView();
    clear_values[clear_value_count].depthStencil.depth =
        desc.depth_attachment.clear_value;
    clear_values[clear_value_count].depthStencil.stencil =
        desc.stencil_attachment.clear_value;
    ++attachment_count;
    ++clear_value_count;
  }

  const auto render_pass_key = BuildLegacyRenderPassKey(
      desc, color_context, depth_stencil_context, has_depth, has_stencil);
  VkRenderPass render_pass =
      state->GetOrCreateLegacyRenderPass(render_pass_key);
  if (render_pass == VK_NULL_HANDLE) {
    return false;
  }

  VkFramebufferCreateInfo framebuffer_info = {};
  framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_info.renderPass = render_pass;
  framebuffer_info.attachmentCount = attachment_count;
  framebuffer_info.pAttachments = attachments.data();
  framebuffer_info.width = desc.GetTargetWidth();
  framebuffer_info.height = desc.GetTargetHeight();
  framebuffer_info.layers = 1;

  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  const VkResult result = state->DeviceFns().vkCreateFramebuffer(
      state->GetLogicalDevice(), &framebuffer_info, nullptr, &framebuffer);
  if (result != VK_SUCCESS || framebuffer == VK_NULL_HANDLE) {
    LOGE("Failed to create Vulkan framebuffer: {}",
         static_cast<int32_t>(result));
    return false;
  }

  VkRenderPassBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin_info.renderPass = render_pass;
  begin_info.framebuffer = framebuffer;
  begin_info.renderArea.offset = {0, 0};
  begin_info.renderArea.extent = {desc.GetTargetWidth(),
                                  desc.GetTargetHeight()};
  begin_info.clearValueCount = clear_value_count;
  begin_info.pClearValues = clear_values.data();

  state->DeviceFns().vkCmdBeginRenderPass(command_buffer.GetCommandBuffer(),
                                          &begin_info,
                                          VK_SUBPASS_CONTENTS_INLINE);
  const bool recorded =
      RecordDrawCommands(state, command_buffer, pass, viewport, scissor);
  state->DeviceFns().vkCmdEndRenderPass(command_buffer.GetCommandBuffer());
  if (!recorded) {
    return false;
  }

  TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                        *color_context.texture, color_context.final_layout);
  if (color_context.resolve_texture != nullptr) {
    TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                          *color_context.resolve_texture,
                          color_context.resolve_texture->GetPreferredLayout());
  }
  if (depth_stencil_context.texture != nullptr) {
    TransitionImageLayout(*state, command_buffer.GetCommandBuffer(),
                          *depth_stencil_context.texture,
                          depth_stencil_context.final_layout);
  }

  std::weak_ptr<const VulkanContextState> weak_state = state;
  command_buffer.RecordCleanupAction([weak_state, framebuffer]() {
    auto state = weak_state.lock();
    if (state == nullptr || state->GetLogicalDevice() == VK_NULL_HANDLE) {
      return;
    }

    if (framebuffer != VK_NULL_HANDLE &&
        state->DeviceFns().vkDestroyFramebuffer != nullptr) {
      state->DeviceFns().vkDestroyFramebuffer(state->GetLogicalDevice(),
                                              framebuffer, nullptr);
    }
  });
  const auto color_texture_ref = desc.color_attachment.texture;
  const auto resolve_texture_ref = desc.color_attachment.resolve_texture;
  const auto depth_texture_ref = desc.depth_attachment.texture;
  const auto stencil_texture_ref = desc.stencil_attachment.texture;
  command_buffer.RecordCleanupAction([color_texture_ref, resolve_texture_ref,
                                      depth_texture_ref,
                                      stencil_texture_ref]() {});

  return true;
}

}  // namespace

GPURenderPassVK::GPURenderPassVK(
    std::shared_ptr<const VulkanContextState> state,
    GPUCommandBufferVK* command_buffer, const GPURenderPassDescriptor& desc)
    : GPURenderPass(desc),
      state_(std::move(state)),
      command_buffer_(command_buffer) {}

void GPURenderPassVK::EncodeCommands(std::optional<GPUViewport> viewport,
                                     std::optional<GPUScissorRect> scissor) {
  if (state_ == nullptr || command_buffer_ == nullptr) {
    LOGE("Failed to encode Vulkan render pass: invalid state");
    return;
  }

  if (state_->IsDynamicRenderingEnabled()) {
    RecordDynamicRenderingPass(state_, *command_buffer_, *this, viewport,
                               scissor);
    return;
  }

  RecordLegacyRenderPass(state_, *command_buffer_, *this, viewport, scissor);
}

}  // namespace skity
