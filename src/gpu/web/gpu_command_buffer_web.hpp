// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_GPU_COMMAND_BUFFER_WEB_HPP
#define SRC_GPU_WEB_GPU_COMMAND_BUFFER_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_command_buffer.hpp"

namespace skity {

class GPUCommandBufferWEB : public GPUCommandBuffer {
 public:
  GPUCommandBufferWEB(WGPUDevice device, WGPUQueue queue,
                      WGPUCommandEncoder encoder);
  ~GPUCommandBufferWEB() override;

  std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor &desc) override;

  std::shared_ptr<GPUBlitPass> BeginBlitPass() override;

  bool Submit() override;

  void RecordStageBuffer(WGPUBuffer buffer);
  void RecordBindGroup(WGPUBindGroup bind_group);

 private:
  WGPUDevice device_;
  WGPUQueue queue_;
  WGPUCommandEncoder encoder_;

  std::vector<WGPUBuffer> stage_buffers_;
  std::vector<WGPUBindGroup> bind_groups_;
};

}  // namespace skity

#endif  // SRC_GPU_WEB_GPU_COMMAND_BUFFER_WEB_HPP
