// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_GPU_TEXTURE_WEB_HPP
#define SRC_GPU_WEB_GPU_TEXTURE_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_texture.hpp"

namespace skity {

class GPUDeviceWEB;

class GPUTextureWEB : public GPUTexture,
                      public std::enable_shared_from_this<GPUTextureWEB> {
 public:
  GPUTextureWEB(const GPUTextureDescriptor& descriptor, GPUDeviceWEB* device,
                WGPUTexture texture);

  ~GPUTextureWEB() override;

  size_t GetBytes() const override;

  void UploadData(uint32_t offset_x, uint32_t offset_y, uint32_t width,
                  uint32_t height, void* data) override;

  WGPUTextureView GetTextureView();

  WGPUTexture GetTexture() const { return texture_; }

  static std::shared_ptr<GPUTexture> Create(GPUDeviceWEB* device,
                                            const GPUTextureDescriptor& desc);

 private:
  GPUDeviceWEB* device_ = nullptr;
  WGPUTexture texture_ = nullptr;
  WGPUTextureView texture_view_ = nullptr;
};

}  // namespace skity

#endif  // SRC_GPU_WEB_GPU_TEXTURE_WEB_HPP
