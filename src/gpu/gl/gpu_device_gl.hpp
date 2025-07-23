// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_DEVICE_GL_HPP
#define SRC_GPU_GL_GPU_DEVICE_GL_HPP

#include <memory>

#include "src/gpu/gpu_device.hpp"

namespace skity {

class GPUDeviceGL : public GPUDevice {
 public:
  GPUDeviceGL();

  ~GPUDeviceGL() override;

  std::unique_ptr<GPUBuffer> CreateBuffer(GPUBufferUsageMask usage) override;

  std::shared_ptr<GPUShaderFunction> CreateShaderFunction(
      const GPUShaderFunctionDescriptor& desc) override;

  std::unique_ptr<GPURenderPipeline> CreateRenderPipeline(
      const GPURenderPipelineDescriptor& desc) override;

  std::unique_ptr<GPURenderPipeline> ClonePipeline(
      GPURenderPipeline* base,
      const GPURenderPipelineDescriptor& desc) override;

  std::shared_ptr<GPUCommandBuffer> CreateCommandBuffer() override;

  std::shared_ptr<GPUSampler> CreateSampler(
      const GPUSamplerDescriptor& desc) override;

  std::shared_ptr<GPUTexture> CreateTexture(
      const GPUTextureDescriptor& desc) override;

  bool CanUseMSAA() override;

  uint32_t GetBufferAlignment() override;

  uint32_t GetMaxTextureSize() override;

  std::shared_ptr<GPUShaderFunction> CreateShaderFunctionFromModule(
      const GPUShaderFunctionDescriptor& desc);

 private:
  void InitGLVersion();

 private:
  uint32_t ubo_offset_ = 0;
  uint32_t max_texture_size_ = 0;
  GPUSamplerMap sampler_map_;
  int32_t gl_version_major_ = 0;
  int32_t gl_version_minor_ = 0;
  bool is_gles_ = false;
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_DEVICE_GL_HPP
