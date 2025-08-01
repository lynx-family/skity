// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_COMMAND_BUFFER_HPP
#define SRC_GPU_GPU_COMMAND_BUFFER_HPP

#include <memory>
#include <vector>

#include "src/gpu/gpu_blit_pass.hpp"
#include "src/gpu/gpu_render_pass.hpp"

namespace skity {

class GPUCommandBuffer {
 public:
  virtual ~GPUCommandBuffer() = default;

  virtual std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor& desc) = 0;

  virtual std::shared_ptr<GPUBlitPass> BeginBlitPass() = 0;

  virtual bool Submit() = 0;
};

class GPUCommandBufferProxy : public GPUCommandBuffer {
 public:
  explicit GPUCommandBufferProxy(
      std::shared_ptr<GPUCommandBuffer> command_buffer)
      : command_buffer_(command_buffer) {}

  std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor& desc) override {
    auto render_pass = std::make_shared<GPURenderPassProxy>(desc);
    render_passes_.push_back(render_pass);
    return render_pass;
  }

  std::shared_ptr<GPUBlitPass> BeginBlitPass() override { return nullptr; }

  bool Submit() override {
    for (auto& render_pass_proxy : render_passes_) {
      auto render_pass =
          command_buffer_->BeginRenderPass(render_pass_proxy->GetDescriptor());
      for (auto& command : render_pass_proxy->GetCommands()) {
        render_pass->AddCommand(command);
      }
      render_pass->EncodeCommands(render_pass_proxy->viewport_,
                                  render_pass_proxy->scissor_);
    }
    return command_buffer_->Submit();
  }

 private:
  std::vector<std::shared_ptr<GPURenderPassProxy>> render_passes_;
  std::shared_ptr<GPUCommandBuffer> command_buffer_;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_COMMAND_BUFFER_HPP
