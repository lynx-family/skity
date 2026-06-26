// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_BUFFER_HPP
#define SRC_GPU_GPU_BUFFER_HPP

#include <cstddef>
#include <cstdint>

namespace skity {

class GPUBuffer;
struct GPUBufferView {
  GPUBuffer* buffer = nullptr;
  uint32_t offset = 0;
  uint32_t range = 0;
};

using GPUBufferUsageMask = uint32_t;

enum GPUBufferUsage : uint32_t {
  kVertexBuffer = 0x1,
  kUniformBuffer = (0x1 << 1),
  kIndexBuffer = (0x1 << 2),

  kDefaultBufferUsage = (kVertexBuffer | kUniformBuffer | kIndexBuffer),
};

enum class GPUBufferStorageMode {
  kPrivate,
  kHostVisible,
};

struct GPUBufferDescriptor {
  GPUBufferUsageMask usage = GPUBufferUsage::kDefaultBufferUsage;
  GPUBufferStorageMode storage_mode = GPUBufferStorageMode::kPrivate;
};

class GPUBuffer {
 public:
  explicit GPUBuffer(const GPUBufferDescriptor& desc)
      : usage_(desc.usage), storage_mode_(desc.storage_mode) {}

  virtual ~GPUBuffer() = default;

  GPUBufferUsageMask GetUsage() const { return usage_; }

  GPUBufferStorageMode GetStorageMode() const { return storage_mode_; }

  virtual void* Map(size_t size) {
    (void)size;
    return nullptr;
  }

  virtual void Unmap() {}

 private:
  GPUBufferUsageMask usage_;
  GPUBufferStorageMode storage_mode_;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_BUFFER_HPP
