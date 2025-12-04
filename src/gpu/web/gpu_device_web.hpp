// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_GPU_DEVICE_WEB_HPP
#define SRC_GPU_WEB_GPU_DEVICE_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_device.hpp"

namespace skity {

class GPUDeviceWEB : public GPUDevice {
 public:
  GPUDeviceWEB(WGPUDevice device, WGPUQueue queue);

  ~GPUDeviceWEB() override = default;

  bool CanUseMSAA() override { return true; }

  uint32_t GetBufferAlignment() override {
    return limits_.minUniformBufferOffsetAlignment;
  }

  uint32_t GetMaxTextureSize() override {
    return limits_.maxTextureDimension2D;
  }

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

  std::shared_ptr<GPUShaderModule> CreateShaderModule(
      const GPUShaderModuleDescriptor& desc) override;

  WGPUDevice GetDevice() const { return device_; }

 private:
  WGPUDevice device_;
  WGPUQueue queue_;
  WGPULimits limits_ = {};
};

}  // namespace skity

#endif  // SRC_GPU_WEB_GPU_DEVICE_WEB_HPP
