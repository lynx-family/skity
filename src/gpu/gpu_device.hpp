// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_DEVICE_HPP
#define SRC_GPU_GPU_DEVICE_HPP

#include <memory>
#include <vector>

#include "src/gpu/gpu_buffer.hpp"
#include "src/gpu/gpu_command_buffer.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_shader_function.hpp"
#include "src/gpu/gpu_shader_module.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

class GPUDevice {
 public:
  GPUDevice() = default;

  virtual ~GPUDevice() = default;

  virtual std::unique_ptr<GPUBuffer> CreateBuffer(GPUBufferUsageMask usage) = 0;

  virtual std::shared_ptr<GPUShaderFunction> CreateShaderFunction(
      const GPUShaderFunctionDescriptor& desc) = 0;

  virtual std::unique_ptr<GPURenderPipeline> CreateRenderPipeline(
      const GPURenderPipelineDescriptor& desc) = 0;

  virtual std::unique_ptr<GPURenderPipeline> ClonePipeline(
      GPURenderPipeline* base, const GPURenderPipelineDescriptor& desc) = 0;

  virtual std::shared_ptr<GPUCommandBuffer> CreateCommandBuffer() = 0;

  virtual std::shared_ptr<GPUSampler> CreateSampler(
      const GPUSamplerDescriptor& desc) = 0;

  virtual std::shared_ptr<GPUTexture> CreateTexture(
      const GPUTextureDescriptor& desc) = 0;

  /**
   * This function is only used in GL backend in Android, to check if
   * `EXT_multisample_render_to_texture` is available, other backends can just
   * return true
   *
   */
  virtual bool CanUseMSAA() = 0;

  /**
   * Get the minimal alignment required by uniform buffer
   *
   * @return offset value for GPU buffer binding to uniform slot
   */
  virtual uint32_t GetBufferAlignment() = 0;

  /**
   * Get the max size of texture allowed in GPU
   *
   * @return max texture size
   */
  virtual uint32_t GetMaxTextureSize() = 0;

  virtual std::shared_ptr<GPUShaderModule> CreateShaderModule(
      const GPUShaderModuleDescriptor& desc);
};

}  // namespace skity

#endif  // SRC_GPU_GPU_DEVICE_HPP
