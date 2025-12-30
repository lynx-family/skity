// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "src/gpu/mtl/gpu_command_buffer_mtl.h"

#include <memory>

#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/mtl/formats_mtl.h"
#include "src/gpu/mtl/gpu_blit_pass_mtl.h"
#include "src/gpu/mtl/gpu_render_pass_mtl.h"
#include "src/logging.hpp"

namespace skity {

std::shared_ptr<GPURenderPass> GPUCommandBufferMTL::BeginRenderPass(
    const GPURenderPassDescriptor& desc) {
  id<MTLRenderCommandEncoder> encoder =
      [mtl_command_buffer_ renderCommandEncoderWithDescriptor:ToMTLRenderPassDescriptor(desc)];
  return std::make_shared<GPURenderPassMTL>(encoder, desc, /*auto_end_encoding=*/true);
}

std::shared_ptr<GPUBlitPass> GPUCommandBufferMTL::BeginBlitPass() {
  id<MTLBlitCommandEncoder> blit_encoder = [mtl_command_buffer_ blitCommandEncoder];
  return std::make_shared<GPUBlitPassMTL>(mtl_device_, blit_encoder);
}

bool GPUCommandBufferMTL::Submit() {
  mtl_command_buffer_.label = [NSString stringWithUTF8String:GetLabel().c_str()];
  [mtl_command_buffer_ addCompletedHandler:^(id<MTLCommandBuffer> cmd) {
    if (cmd.error == nil) {
      return;
    }

    auto label = cmd.label ? cmd.label.UTF8String : "";
    LOGE("CMD {} failed: {}", label, [[cmd.error localizedDescription] UTF8String]);
  }];
  [mtl_command_buffer_ commit];
  return true;
}

}  // namespace skity
