
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
  GPUBufferMTL(const GPUBufferDescriptor& desc, id<MTLDevice> device,
               id<MTLCommandQueue> queue);

  id<MTLBuffer> GetMTLBuffer() const { return mtl_buffer_; }

  void* Map(size_t size) override;
  void Unmap() override;

  void RecreateBufferIfNeeded(size_t size);

 private:
  MTLResourceOptions GetMTLResourceOptions() const;

  bool UsesManagedStorage() const;

  id<MTLDevice> device_;
  id<MTLCommandQueue> queue_;
  id<MTLBuffer> mtl_buffer_;
  size_t mapped_size_ = 0;
  bool mapped_ = false;
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_BUFFER_MTL_HPP
