// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "src/gpu/mtl/gpu_buffer_mtl.h"

#include <skity/macros.hpp>

#include "src/logging.hpp"

namespace skity {

GPUBufferMTL::GPUBufferMTL(const GPUBufferDescriptor& desc, id<MTLDevice> device,
                           id<MTLCommandQueue> queue)
    : GPUBuffer(desc), device_(device), queue_(queue) {}

void* GPUBufferMTL::Map(size_t size) {
  if (GetStorageMode() != GPUBufferStorageMode::kHostVisible || size == 0) {
    return nullptr;
  }

  if (mapped_) {
    LOGE("GPUBufferMTL::Map called while buffer is already mapped");
    return nullptr;
  }

  id<MTLBuffer> new_buffer = [device_ newBufferWithLength:size options:GetMTLResourceOptions()];
  if (new_buffer == nil) {
    LOGE("Failed to create host visible MTLBuffer with size {}, maybe out of "
         "memory?",
         size);
    return nullptr;
  }

  mtl_buffer_ = new_buffer;
  mapped_ = true;
  mapped_size_ = size;
  return [mtl_buffer_ contents];
}

void GPUBufferMTL::Unmap() {
  if (!mapped_) {
    return;
  }

#if !defined(SKITY_IOS)
  if (UsesManagedStorage() && mtl_buffer_ != nil && mapped_size_ > 0) {
    [mtl_buffer_ didModifyRange:NSMakeRange(0, mapped_size_)];
  }
#endif

  mapped_ = false;
  mapped_size_ = 0;
}

void GPUBufferMTL::RecreateBufferIfNeeded(size_t size) {
  if (mtl_buffer_.length < size) {
    mtl_buffer_ = [device_ newBufferWithLength:size options:GetMTLResourceOptions()];

    if (mtl_buffer_ == nil) {
      LOGE("Failed to create MTLBuffer with size {}, maybe out of memory?", size);
    }
  }
}

MTLResourceOptions GPUBufferMTL::GetMTLResourceOptions() const {
  if (GetStorageMode() != GPUBufferStorageMode::kHostVisible) {
    return MTLResourceStorageModePrivate;
  }

#if defined(SKITY_IOS)
  return MTLResourceStorageModeShared;
#else
  if (@available(macos 10.15, *)) {
    return device_.hasUnifiedMemory ? MTLResourceStorageModeShared : MTLResourceStorageModeManaged;
  }

  return MTLResourceStorageModeManaged;
#endif
}

bool GPUBufferMTL::UsesManagedStorage() const {
#if defined(SKITY_IOS)
  return false;
#else
  return GetMTLResourceOptions() == MTLResourceStorageModeManaged;
#endif
}

}  // namespace skity
