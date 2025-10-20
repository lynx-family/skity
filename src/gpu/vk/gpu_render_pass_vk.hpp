// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_RENDER_PASS_VK_HPP
#define SRC_GPU_VK_GPU_RENDER_PASS_VK_HPP

#include <volk.h>

#include "src/gpu/gpu_render_pass.hpp"

namespace skity {

class GPUCommandBufferVk;
class GPUDeviceVk;

class GPURenderPassVk : public GPURenderPass {
 public:
  GPURenderPassVk(GPUCommandBufferVk* command_buffer,
                  const GPURenderPassDescriptor& desc);
  ~GPURenderPassVk() override;

  void EncodeCommands(
      std::optional<GPUViewport> viewport = std::nullopt,
      std::optional<GPUScissorRect> scissor = std::nullopt) override;

 private:
  bool CreateVkRenderPass();
  void SetupViewportAndScissor(std::optional<GPUViewport> viewport,
                               std::optional<GPUScissorRect> scissor);
  void ExecuteCommands();
  void ExecuteSingleCommand(VkCommandBuffer cmd_buffer, const Command* command);

  GPUCommandBufferVk* command_buffer_ = nullptr;
  VkRenderPass vk_render_pass_ = VK_NULL_HANDLE;
  VkFramebuffer vk_framebuffer_ = VK_NULL_HANDLE;
  bool render_pass_created_ = false;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_RENDER_PASS_VK_HPP
