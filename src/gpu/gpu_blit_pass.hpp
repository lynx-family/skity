// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_BLIT_PASS_HPP
#define SRC_GPU_GPU_BLIT_PASS_HPP

#include "src/gpu/gpu_buffer.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

class GPUBlitPass {
 public:
  virtual void UploadTextureData(std::shared_ptr<GPUTexture> texture,
                                 uint32_t offset_x, uint32_t offset_y,
                                 uint32_t width, uint32_t height,
                                 void* data) = 0;

  virtual void UploadBufferData(GPUBuffer* buffer, void* data, size_t size) = 0;

  virtual void End() = 0;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_BLIT_PASS_HPP
