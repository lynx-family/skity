// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "src/gpu/mtl/gpu_buffer_mtl.h"
#include "src/logging.hpp"

namespace skity {

GPUBufferMTL::GPUBufferMTL(GPUBufferUsageMask usage, id<MTLDevice> device,
                           id<MTLCommandQueue> queue)
    : GPUBuffer(usage), device_(device), queue_(queue) {}

void GPUBufferMTL::RecreateBufferIfNeeded(size_t size) {
  if (mtl_buffer_.length < size) {
    mtl_buffer_ = [device_ newBufferWithLength:size options:MTLResourceStorageModePrivate];

    if (mtl_buffer_ == nil) {
      LOGE("Failed to create MTLBuffer with size {}, mabye out of memory?", size);
    }
  }
}

}  // namespace skity
