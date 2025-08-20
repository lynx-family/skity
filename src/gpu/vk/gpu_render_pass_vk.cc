// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_render_pass_vk.hpp"

#include "src/gpu/vk/gpu_buffer_vk.hpp"
#include "src/gpu/vk/gpu_command_buffer_vk.hpp"
#include "src/gpu/vk/gpu_descriptor_set_vk.hpp"
#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/gpu_render_pipeline_vk.hpp"
#include "src/gpu/vk/gpu_texture_vk.hpp"
#include "src/logging.hpp"

namespace skity {

GPURenderPassVk::GPURenderPassVk(GPUCommandBufferVk* command_buffer,
                                 const GPURenderPassDescriptor& desc)
    : GPURenderPass(desc),
      command_buffer_(command_buffer),
      vk_render_pass_(VK_NULL_HANDLE),
      vk_framebuffer_(VK_NULL_HANDLE),
      render_pass_created_(false) {}

GPURenderPassVk::~GPURenderPassVk() {
  auto* device = command_buffer_->GetDevice();
  VkDevice vk_device = device->GetDevice();

  if (vk_framebuffer_ != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(vk_device, vk_framebuffer_, nullptr);
  }

  if (vk_render_pass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(vk_device, vk_render_pass_, nullptr);
  }
}

void GPURenderPassVk::EncodeCommands(std::optional<GPUViewport> viewport,
                                     std::optional<GPUScissorRect> scissor) {
  const auto& commands = GetCommands();
  LOGI("Encoding %zu render commands", commands.size());

  // For now, we'll implement a basic render pass without actual attachments
  // In a full implementation, we would:
  // 1. Create VkRenderPass and VkFramebuffer based on descriptor
  // 2. Begin render pass with vkCmdBeginRenderPass
  // 3. Set viewport and scissor
  // 4. Execute each render command
  // 5. End render pass with vkCmdEndRenderPass

  if (!render_pass_created_) {
    if (!CreateVkRenderPass()) {
      LOGE("Failed to create Vulkan render pass");
      return;
    }
    render_pass_created_ = true;
  }

  // Get the command buffer
  VkCommandBuffer cmd_buffer = command_buffer_->GetVkCommandBuffer();

  // Set viewport and scissor
  SetupViewportAndScissor(viewport, scissor);

  // Execute render commands
  ExecuteCommands();

  LOGI("Render pass encoding completed");
}

bool GPURenderPassVk::CreateVkRenderPass() {
  const auto& desc = GetDescriptor();
  auto* device = command_buffer_->GetDevice();
  VkDevice vk_device = device->GetDevice();

  // Check if we have a color attachment
  if (!desc.color_attachment.texture) {
    LOGE("No color attachment texture provided for render pass");
    return false;
  }

  // Get the Vulkan texture from the color attachment
  auto* texture_vk =
      static_cast<GPUTextureVk*>(desc.color_attachment.texture.get());
  if (!texture_vk) {
    LOGE("Failed to cast to Vulkan texture");
    return false;
  }

  // Get texture format from the actual texture
  VkFormat texture_format = texture_vk->GetVkFormat();
  LOGI("Render pass texture format: %d (expected swapchain compatible format)",
       texture_format);

  // Create render pass with color attachment
  VkAttachmentDescription color_attachment{};
  color_attachment.format = texture_format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = (desc.color_attachment.load_op == GPULoadOp::kClear)
                                ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                : VK_ATTACHMENT_LOAD_OP_LOAD;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  // For swapchain images, final layout must be PRESENT_SRC_KHR for presentation
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // Add depth/stencil attachment for path rendering (like GL backend)
  VkAttachmentDescription depth_stencil_attachment{};
  VkAttachmentReference depth_stencil_ref{};
  std::vector<VkAttachmentDescription> attachments = {color_attachment};

  bool has_depth_stencil = desc.stencil_attachment.texture != nullptr ||
                           desc.depth_attachment.texture != nullptr;
  if (has_depth_stencil) {
    depth_stencil_attachment.format =
        VK_FORMAT_D24_UNORM_S8_UINT;  // Combined depth-stencil format
    depth_stencil_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_stencil_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_stencil_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_stencil_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_stencil_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_stencil_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_stencil_attachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    depth_stencil_ref.attachment = 1;  // Second attachment
    depth_stencil_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    attachments.push_back(depth_stencil_attachment);
  }

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  if (has_depth_stencil) {
    subpass.pDepthStencilAttachment = &depth_stencil_ref;
  }

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
  render_pass_info.pAttachments = attachments.data();
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  VkResult result = vkCreateRenderPass(vk_device, &render_pass_info, nullptr,
                                       &vk_render_pass_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create Vulkan render pass: %d", result);
    return false;
  }

  // Create framebuffer
  VkImageView image_view = texture_vk->GetVkImageView();
  if (image_view == VK_NULL_HANDLE) {
    LOGE("Texture has no image view for framebuffer creation");
    return false;
  }

  std::vector<VkImageView> framebuffer_attachments = {image_view};

  // Add depth/stencil attachment if available
  if (has_depth_stencil && desc.stencil_attachment.texture) {
    auto* depth_stencil_texture_vk =
        static_cast<GPUTextureVk*>(desc.stencil_attachment.texture.get());
    if (depth_stencil_texture_vk) {
      VkImageView depth_stencil_view =
          depth_stencil_texture_vk->GetVkImageView();
      if (depth_stencil_view != VK_NULL_HANDLE) {
        framebuffer_attachments.push_back(depth_stencil_view);
      }
    }
  }

  VkFramebufferCreateInfo framebuffer_info{};
  framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_info.renderPass = vk_render_pass_;
  framebuffer_info.attachmentCount =
      static_cast<uint32_t>(framebuffer_attachments.size());
  framebuffer_info.pAttachments = framebuffer_attachments.data();
  framebuffer_info.width = desc.GetTargetWidth();
  framebuffer_info.height = desc.GetTargetHeight();
  framebuffer_info.layers = 1;

  result = vkCreateFramebuffer(vk_device, &framebuffer_info, nullptr,
                               &vk_framebuffer_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create framebuffer: %d", result);
    return false;
  }

  LOGI("Successfully created Vulkan render pass and framebuffer (%ux%u)",
       desc.GetTargetWidth(), desc.GetTargetHeight());
  return true;
}

void GPURenderPassVk::SetupViewportAndScissor(
    std::optional<GPUViewport> viewport,
    std::optional<GPUScissorRect> scissor) {
  VkCommandBuffer cmd_buffer = command_buffer_->GetVkCommandBuffer();

  // Set viewport if provided
  if (viewport.has_value()) {
    VkViewport vk_viewport = {};
    vk_viewport.x = viewport->x;
    // Vulkan viewport Y is from top-left, but we need to handle flipping
    // properly
    vk_viewport.y = viewport->y;
    vk_viewport.width = viewport->width;
    vk_viewport.height = viewport->height;
    vk_viewport.minDepth = viewport->min_depth;
    vk_viewport.maxDepth = viewport->max_depth;

    vkCmdSetViewport(cmd_buffer, 0, 1, &vk_viewport);
  }

  // Set scissor if provided
  if (scissor.has_value()) {
    VkRect2D vk_scissor = {};
    vk_scissor.offset.x = static_cast<int32_t>(scissor->x);
    vk_scissor.offset.y = static_cast<int32_t>(scissor->y);
    vk_scissor.extent.width = scissor->width;
    vk_scissor.extent.height = scissor->height;

    vkCmdSetScissor(cmd_buffer, 0, 1, &vk_scissor);
    LOGI("Set scissor: %ux%u at (%u, %u)", scissor->width, scissor->height,
         scissor->x, scissor->y);
  }
}

void GPURenderPassVk::ExecuteCommands() {
  auto& commands = const_cast<ArrayList<Command*, 32>&>(GetCommands());
  VkCommandBuffer cmd_buffer = command_buffer_->GetVkCommandBuffer();

  if (commands.empty()) {
    LOGI("No render commands to execute");
    return;
  }

  LOGI("Executing %zu render commands", commands.size());

  // Begin render pass if we have a valid one
  if (vk_render_pass_ != VK_NULL_HANDLE && vk_framebuffer_ != VK_NULL_HANDLE) {
    const auto& desc = GetDescriptor();

    // For window surfaces (swapchain), image layout transitions are handled
    // differently The swapchain images are already managed by the window
    // surface
    if (desc.color_attachment.texture) {
      auto* texture_vk =
          static_cast<GPUTextureVk*>(desc.color_attachment.texture.get());
      if (texture_vk && texture_vk->GetVkImage() != VK_NULL_HANDLE) {
        // Only transition non-swapchain textures
        LOGI(
            "Skipping layout transition for render target (likely swapchain "
            "image)");
      } else {
        LOGI(
            "No color attachment texture or invalid texture for layout "
            "transition");
      }
    }

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = vk_render_pass_;
    begin_info.framebuffer = vk_framebuffer_;
    begin_info.renderArea.offset = {0, 0};
    begin_info.renderArea.extent = {desc.GetTargetWidth(),
                                    desc.GetTargetHeight()};

    // Set up clear values for all attachments that need clearing
    std::vector<VkClearValue> clear_values;

    // Color attachment clear value
    VkClearValue color_clear = {};
    if (desc.color_attachment.load_op == GPULoadOp::kClear) {
      // Use the actual clear color from the attachment
      color_clear.color = {
          {static_cast<float>(desc.color_attachment.clear_value.r),
           static_cast<float>(desc.color_attachment.clear_value.g),
           static_cast<float>(desc.color_attachment.clear_value.b),
           static_cast<float>(desc.color_attachment.clear_value.a)}};
    }
    clear_values.push_back(color_clear);

    // Depth/stencil attachment clear value if present
    bool has_depth_stencil = desc.stencil_attachment.texture != nullptr ||
                             desc.depth_attachment.texture != nullptr;
    if (has_depth_stencil) {
      VkClearValue depth_stencil_clear = {};
      depth_stencil_clear.depthStencil.depth =
          desc.depth_attachment.clear_value;  // 0.0f
      depth_stencil_clear.depthStencil.stencil =
          desc.stencil_attachment.clear_value;  // 0
      clear_values.push_back(depth_stencil_clear);
    }

    begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    LOGI("Started render pass (%ux%u)", desc.GetTargetWidth(),
         desc.GetTargetHeight());
  }

  // Execute each command
  for (size_t i = 0; i < commands.size(); ++i) {
    const Command* command = commands[i];
    if (!command || !command->IsValid()) {
      LOGW("Skipping invalid command %zu", i);
      continue;
    }

    ExecuteSingleCommand(cmd_buffer, command);
  }

  // End render pass if we began one
  if (vk_render_pass_ != VK_NULL_HANDLE && vk_framebuffer_ != VK_NULL_HANDLE) {
    vkCmdEndRenderPass(cmd_buffer);
    LOGI("Ended render pass");
  }

  LOGI("Completed executing %zu render commands", commands.size());
}

void GPURenderPassVk::ExecuteSingleCommand(VkCommandBuffer cmd_buffer,
                                           const Command* command) {
  LOGI("ExecuteSingleCommand called");
  if (!command || !command->pipeline) {
    LOGW("Invalid command or pipeline");
    return;
  }

  // Get the Vulkan pipeline from the command
  auto* pipeline_vk = static_cast<GPURenderPipelineVk*>(command->pipeline);
  if (!pipeline_vk || !pipeline_vk->IsValid()) {
    LOGW("Invalid Vulkan pipeline");
    return;
  }

  // Bind graphics pipeline
  VkPipeline vk_pipeline = pipeline_vk->GetVkPipeline();
  vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);

  // Bind vertex buffer
  if (command->vertex_buffer.buffer) {
    auto* vertex_buffer_vk =
        static_cast<GPUBufferVk*>(command->vertex_buffer.buffer);
    VkBuffer vertex_buffer = vertex_buffer_vk->GetBuffer();
    VkDeviceSize offset = command->vertex_buffer.offset;
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vertex_buffer, &offset);
  }

  // Bind index buffer
  if (command->index_buffer.buffer) {
    auto* index_buffer_vk =
        static_cast<GPUBufferVk*>(command->index_buffer.buffer);
    VkBuffer index_buffer = index_buffer_vk->GetBuffer();
    VkDeviceSize offset = command->index_buffer.offset;
    VkIndexType index_type = VK_INDEX_TYPE_UINT32;  // Indices are uint32_t
    vkCmdBindIndexBuffer(cmd_buffer, index_buffer, offset, index_type);
  }

  // Bind descriptor sets (uniform buffers, textures, samplers)
  LOGI(
      "Command has %zu uniform bindings, %zu texture_sampler bindings, %zu "
      "sampler bindings",
      command->uniform_bindings.size(),
      command->texture_sampler_bindings.size(),
      command->sampler_bindings.size());

  if (!command->uniform_bindings.empty() ||
      !command->texture_sampler_bindings.empty() ||
      !command->sampler_bindings.empty()) {
    // Use the pipeline's pre-computed descriptor layout from shader reflection
    // But also handle cases where commands have more bindings than shader
    // reflection found

    // Use the pipeline's stored bindings that match the SPIRV layout (0,1,2,3)
    // This ensures descriptor set layout matches pipeline layout exactly
    auto descriptor_set = pipeline_vk->CreateDescriptorSetUsingPipelineLayout();
    if (descriptor_set) {
      // Update descriptor set with actual resources from command bindings
      for (const auto& uniform_binding : command->uniform_bindings) {
        if (uniform_binding.buffer.buffer) {
          auto* buffer_vk =
              static_cast<GPUBufferVk*>(uniform_binding.buffer.buffer);
          descriptor_set->BindBuffer(uniform_binding.index, buffer_vk,
                                     uniform_binding.buffer.offset,
                                     uniform_binding.buffer.range);
        }
      }

      for (const auto& texture_binding : command->texture_sampler_bindings) {
        if (texture_binding.texture) {
          auto* texture_vk =
              static_cast<GPUTextureVk*>(texture_binding.texture.get());
          descriptor_set->BindTexture(texture_binding.index, texture_vk,
                                      texture_binding.sampler.get());
        }
      }

      // CRITICAL: Update the descriptor set with bound resources
      if (!descriptor_set->UpdateDescriptorSet()) {
        LOGE("Failed to update descriptor set");
      }

      // Bind the descriptor set to the command buffer
      pipeline_vk->BindDescriptorSet(cmd_buffer, descriptor_set);
      LOGI("Bound descriptor set using pipeline's reflection-based layout");
    } else {
      LOGW("Failed to create descriptor set using pipeline layout");
    }
  } else {
    // If there are no bindings, this indicates a problem in the rendering
    // pipeline The shaders expect CommonSlot uniform data but none was provided
    LOGW(
        "No uniform or texture bindings provided - this indicates missing "
        "CommonSlot data");
    LOGW("This will likely cause descriptor set validation errors");
  }

  // Set scissor for this command if specified
  if (command->scissor_rect.width > 0 && command->scissor_rect.height > 0) {
    VkRect2D scissor = {};
    scissor.offset.x = static_cast<int32_t>(command->scissor_rect.x);
    scissor.offset.y = static_cast<int32_t>(command->scissor_rect.y);
    scissor.extent.width = static_cast<uint32_t>(command->scissor_rect.width);
    scissor.extent.height = static_cast<uint32_t>(command->scissor_rect.height);

    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);
  }

  // Set stencil reference value from command (like GL and Metal backends)
  // Only set it if the pipeline has stencil testing enabled and dynamic stencil
  // reference
  if (pipeline_vk->HasStencilTesting()) {
    vkCmdSetStencilReference(cmd_buffer, VK_STENCIL_FACE_FRONT_AND_BACK,
                             command->stencil_reference);
  }

  // Draw indexed
  if (command->index_count > 0) {
    vkCmdDrawIndexed(cmd_buffer, command->index_count, 1, 0, 0, 0);
    LOGI("Drew %u indices", command->index_count);
  } else {
    LOGW(
        "Command has no indices to draw - this means geometry was not "
        "generated properly");
  }
}

}  // namespace skity