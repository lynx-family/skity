// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_COMMAND_BUFFER_MTL_H
#define SRC_GPU_MTL_GPU_COMMAND_BUFFER_MTL_H

#import <Metal/Metal.h>

#include "src/gpu/gpu_command_buffer.hpp"

namespace skity {

class GPUCommandBufferMTL : public GPUCommandBuffer {
 public:
  explicit GPUCommandBufferMTL(id<MTLCommandBuffer> mtl_command_buffer)
      : mtl_command_buffer_(mtl_command_buffer) {}

  id<MTLCommandBuffer> GetMTLCommandBuffer() const {
    return mtl_command_buffer_;
  }

  std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor& desc) override;

  bool Submit() override;

 private:
  id<MTLCommandBuffer> mtl_command_buffer_;
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_COMMAND_BUFFER_MTL_H
