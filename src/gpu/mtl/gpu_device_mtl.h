// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_DEVICE_MTL_HPP
#define SRC_GPU_MTL_GPU_DEVICE_MTL_HPP

#import <Metal/Metal.h>

#include <memory>

#include "src/gpu/gpu_device.hpp"
#include "src/gpu/mtl/gpu_render_pipeline_mtl.h"

namespace skity {

class GPUDeviceMTL : public GPUDevice {
 public:
  GPUDeviceMTL(id<MTLDevice> device, id<MTLCommandQueue> queue);

  ~GPUDeviceMTL() override;

  bool CanUseMSAA() override;

  uint32_t GetBufferAlignment() override;

  uint32_t GetMaxTextureSize() override;

  id<MTLDevice> GetMTLDevice() { return mtl_device_; }
  id<MTLCommandQueue> GetMTLCommandQueue() { return mtl_command_queue_; }

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

  bool IsSupportsMemoryless() const { return supports_memoryless_; }

  id<MTLDepthStencilState> FindOrCreateDepthStencilState(
      const GPUDepthStencilState& depth_stencil);

  std::shared_ptr<GPUShaderFunction> CreateShaderFunctionFromModule(
      const GPUShaderFunctionDescriptor& desc);

 private:
  id<MTLDevice> mtl_device_;
  id<MTLCommandQueue> mtl_command_queue_;
  bool supports_memoryless_;
  GPUSamplerMap sampler_map_;
  GPUDepthStencilMap depth_stencil_map_;
  uint32_t max_texture_size_;
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_DEVICE_MTL_HPP
