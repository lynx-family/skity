// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_COMMAND_BUFFER_GL_HPP
#define SRC_GPU_GL_GPU_COMMAND_BUFFER_GL_HPP

#include "src/gpu/gpu_command_buffer.hpp"

namespace skity {

class GPUCommandBufferGL : public GPUCommandBuffer {
 public:
  explicit GPUCommandBufferGL(bool support_msaa)
      : context_support_msaa_(support_msaa) {}

  ~GPUCommandBufferGL() override = default;

  std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor& desc) override;

  bool Submit() override;

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
  bool context_support_msaa_;
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_COMMAND_BUFFER_GL_HPP
