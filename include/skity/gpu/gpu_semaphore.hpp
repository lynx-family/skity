// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_SEMAPHORE_HPP
#define INCLUDE_SKITY_GPU_GPU_SEMAPHORE_HPP

#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * Abstract GPU synchronization semaphore.
 *
 * Used for GPU-GPU synchronization between different GPU contexts or APIs.
 * Create via GPUContext::CreateSemaphore(), import a native sync handle via
 * GPUContext::ImportSemaphore(), then pass to
 * GPUSurface::AddExternalWaitSemaphore() before flushing.
 *
 * The semaphore can be reused across frames: each call to ImportSemaphore()
 * re-imports a new handle into the same underlying GPU semaphore object.
 */
class SKITY_API GPUSemaphore {
 public:
  virtual ~GPUSemaphore() = default;

  GPUBackendType GetBackendType() const { return backend_; }

 protected:
  explicit GPUSemaphore(GPUBackendType backend) : backend_(backend) {}

  GPUBackendType backend_ = GPUBackendType::kNone;
};

/**
 * Base descriptor for importing a native sync handle into a GPUSemaphore.
 *
 * Follows the same polymorphic struct pattern as GPUBackendTextureInfo.
 * Use the backend-specific derived struct (e.g. GPUSemaphoreImportInfoVK)
 * matching the GPUContext backend type.
 */
struct GPUSemaphoreImportInfo {
  GPUBackendType backend = GPUBackendType::kNone;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_SEMAPHORE_HPP
