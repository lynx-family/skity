// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_RENDER_PASS_VK_HPP
#define SRC_GPU_VK_GPU_RENDER_PASS_VK_HPP

#include "src/gpu/gpu_render_pass.hpp"

namespace skity {

class GPUCommandBufferVK;
class VulkanContextState;

class GPURenderPassVK : public GPURenderPass {
 public:
  GPURenderPassVK(std::shared_ptr<const VulkanContextState> state,
                  GPUCommandBufferVK* command_buffer,
                  const GPURenderPassDescriptor& desc);

  ~GPURenderPassVK() override = default;

  void EncodeCommands(
      std::optional<GPUViewport> viewport = std::nullopt,
      std::optional<GPUScissorRect> scissor = std::nullopt) override;

 private:
  std::shared_ptr<const VulkanContextState> state_ = {};
  GPUCommandBufferVK* command_buffer_ = nullptr;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_RENDER_PASS_VK_HPP
