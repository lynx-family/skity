// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_COMMAND_BUFFER_GL_HPP
#define SRC_GPU_GL_GPU_COMMAND_BUFFER_GL_HPP

#include "src/gpu/gpu_command_buffer.hpp"

namespace skity {

class GPUDeviceGL;

class GPUCommandBufferGL : public GPUCommandBuffer {
 public:
  explicit GPUCommandBufferGL(GPUDeviceGL* device) : device_(device) {}

  ~GPUCommandBufferGL() override = default;

  std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor& desc) override;

  std::shared_ptr<GPUBlitPass> BeginBlitPass() override;

  bool Submit(const GPUSubmitInfo* submit_info = nullptr) override;

  std::shared_ptr<GPURenderPass> CreateRenderPassForFBO(
      const GPURenderPassDescriptor& desc, uint32_t fbo_id) const;

 private:
  std::shared_ptr<GPURenderPass> BeginDirectRenderPass(
      const GPURenderPassDescriptor& desc);

  std::shared_ptr<GPURenderPass> BeginMSAAResolveRenderPass(
      const GPURenderPassDescriptor& desc);

#ifdef SKITY_ANDROID
  std::shared_ptr<GPURenderPass> BeginTileMSAARenderPass(
      const GPURenderPassDescriptor& desc);
#endif

 private:
  GPUDeviceGL* device_;
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_COMMAND_BUFFER_GL_HPP
