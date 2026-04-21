// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_DEVICE_VK_HPP
#define SRC_GPU_VK_GPU_DEVICE_VK_HPP

#include "src/gpu/gpu_device.hpp"

namespace skity {

class VulkanContextState;

class GPUDeviceVK : public GPUDevice {
 public:
  explicit GPUDeviceVK(std::shared_ptr<const VulkanContextState> state);

  ~GPUDeviceVK() override = default;

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

  bool CanUseMSAA() override { return true; }

  uint32_t GetBufferAlignment() override { return buffer_alignment_; }

  uint32_t GetMaxTextureSize() override { return max_texture_size_; }

 private:
  std::shared_ptr<GPUShaderFunction> CreateShaderFunctionFromModule(
      const GPUShaderFunctionDescriptor& desc);

  std::shared_ptr<const VulkanContextState> state_ = {};
  uint32_t buffer_alignment_ = 256;
  uint32_t max_texture_size_ = 4096;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_DEVICE_VK_HPP
