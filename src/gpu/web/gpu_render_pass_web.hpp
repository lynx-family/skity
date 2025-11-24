// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_GPU_RENDER_PASS_WEB_HPP
#define SRC_GPU_WEB_GPU_RENDER_PASS_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_render_pass.hpp"

namespace skity {

class GPUCommandBufferWEB;

class GPURenderPassWEB : public GPURenderPass {
 public:
  GPURenderPassWEB(const GPURenderPassDescriptor& desc,
                   GPUCommandBufferWEB* command_buffer, WGPUDevice device,
                   WGPUCommandEncoder encoder);
  ~GPURenderPassWEB() override;

  void EncodeCommands(
      std::optional<GPUViewport> viewport = std::nullopt,
      std::optional<GPUScissorRect> scissor = std::nullopt) override;

 private:
  WGPURenderPassEncoder BeginRenderPass();

  void SetupBindGroup(WGPURenderPassEncoder render_pass,
                      const Command* command);

 private:
  GPUCommandBufferWEB* command_buffer_;
  WGPUDevice device_;
  WGPUCommandEncoder encoder_;
};

}  // namespace skity

#endif  // SRC_GPU_WEB_GPU_RENDER_PASS_WEB_HPP
