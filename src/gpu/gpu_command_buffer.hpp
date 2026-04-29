// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_COMMAND_BUFFER_HPP
#define SRC_GPU_GPU_COMMAND_BUFFER_HPP

#include <memory>
#include <skity/gpu/gpu_backend_type.hpp>
#include <string>
#include <vector>

#include "src/gpu/gpu_blit_pass.hpp"
#include "src/gpu/gpu_render_pass.hpp"

namespace skity {

class GPUSubmitInfo {
 public:
  virtual ~GPUSubmitInfo() = default;

  virtual GPUBackendType GetBackendType() const {
    return GPUBackendType::kNone;
  }
};

class GPUCommandBuffer {
 public:
  virtual ~GPUCommandBuffer() = default;

  virtual std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor& desc) = 0;

  virtual std::shared_ptr<GPUBlitPass> BeginBlitPass() = 0;

  virtual void HoldResource(std::shared_ptr<void> resource) { (void)resource; }

  virtual bool Submit(const GPUSubmitInfo* submit_info = nullptr) = 0;

  void SetLabel(const std::string& label) { label_ = label; }

  const std::string& GetLabel() const { return label_; }

 private:
  std::string label_;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_COMMAND_BUFFER_HPP
