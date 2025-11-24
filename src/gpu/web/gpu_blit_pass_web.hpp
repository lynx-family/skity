// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <webgpu/webgpu.h>

#include <vector>

#include "src/gpu/gpu_blit_pass.hpp"

namespace skity {

class GPUCommandBufferWEB;

class GPUBlitPassWEB : public GPUBlitPass {
 public:
  GPUBlitPassWEB(WGPUDevice device, WGPUCommandEncoder encoder,
                 GPUCommandBufferWEB* command_buffer);

  ~GPUBlitPassWEB() override;

  void UploadTextureData(std::shared_ptr<GPUTexture> texture, uint32_t offset_x,
                         uint32_t offset_y, uint32_t width, uint32_t height,
                         void* data) override;

  void UploadBufferData(GPUBuffer* buffer, void* data, size_t size) override;

  void End() override;

 private:
  WGPUDevice device_;
  WGPUCommandEncoder encoder_;

  GPUCommandBufferWEB* command_buffer_;
};

}  // namespace skity
