
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_BUFFER_MTL_HPP
#define SRC_GPU_MTL_GPU_BUFFER_MTL_HPP

#include <Metal/Metal.h>

#include "src/gpu/gpu_buffer.hpp"

namespace skity {

class GPUBufferMTL : public GPUBuffer {
 public:
  GPUBufferMTL(GPUBufferUsageMask usage, id<MTLDevice> device,
               id<MTLCommandQueue> queue);

  id<MTLBuffer> GetMTLBuffer() const { return mtl_buffer_; }

  void UploadData(void* data, size_t size) override;

 private:
  id<MTLDevice> device_;
  id<MTLCommandQueue> queue_;
  id<MTLBuffer> mtl_buffer_;
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_BUFFER_MTL_HPP
