// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_GPU_RENDER_PIPELINE_WEB_HPP
#define SRC_GPU_WEB_GPU_RENDER_PIPELINE_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_render_pipeline.hpp"

namespace skity {

class GPURenderPipelineWeb : public GPURenderPipeline {
 public:
  GPURenderPipelineWeb(const GPURenderPipelineDescriptor& desc,
                       WGPURenderPipeline pipeline);
  ~GPURenderPipelineWeb() override;

  WGPURenderPipeline GetRenderPipeline() const { return pipeline_; }

  static std::unique_ptr<GPURenderPipeline> Create(
      WGPUDevice device, const GPURenderPipelineDescriptor& desc);

 private:
  WGPURenderPipeline pipeline_;
};

}  // namespace skity

#endif  // SRC_GPU_WEB_GPU_RENDER_PIPELINE_WEB_HPP
