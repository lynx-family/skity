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

class GPUBuffer {
 public:
  explicit GPUBuffer(GPUBufferUsageMask usage) : usage_(usage) {}

  virtual ~GPUBuffer() = default;

  GPUBufferUsageMask GetUsage() const { return usage_; }

 private:
  GPUBufferUsageMask usage_;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_BUFFER_HPP
