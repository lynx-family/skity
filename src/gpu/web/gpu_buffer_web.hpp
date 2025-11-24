// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_GPU_BUFFER_WEB_HPP
#define SRC_GPU_WEB_GPU_BUFFER_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_buffer.hpp"

namespace skity {

class GPUBufferWEB : public GPUBuffer {
 public:
  GPUBufferWEB(GPUBufferUsageMask usage);

  ~GPUBufferWEB() override;

  WGPUBuffer GetBuffer() const { return buffer_; }

  void ResizeIfNeeded(WGPUDevice device, size_t size);

 private:
  WGPUBuffer buffer_ = nullptr;
};

}  // namespace skity

#endif  // SRC_GPU_WEB_GPU_BUFFER_WEB_HPP
