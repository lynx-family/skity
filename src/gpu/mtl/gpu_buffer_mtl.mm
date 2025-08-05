// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/mtl/gpu_buffer_mtl.h"

namespace skity {

GPUBufferMTL::GPUBufferMTL(GPUBufferUsageMask usage, id<MTLDevice> device,
                           id<MTLCommandQueue> queue)
    : GPUBuffer(usage), device_(device), queue_(queue) {}

void GPUBufferMTL::RecreateBufferIfNeeded(size_t size) {
  if (mtl_buffer_.length < size) {
    mtl_buffer_ = [device_ newBufferWithLength:size options:MTLResourceStorageModePrivate];
  }
}

}  // namespace skity
