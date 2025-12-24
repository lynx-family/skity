// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "src/render/hw/mtl/mtl_root_layer.h"

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/mtl/gpu_texture_mtl.h"
#include "src/render/hw/hw_render_pass_builder.hpp"

namespace skity {

HWDrawState MTLRootLayer::OnPrepare(HWDrawContext *context) {
  PrepareAttachments(context);

  auto ret = HWRootLayer::OnPrepare(context);

  PrepareRenderPassDesc(context);

  return ret;
}

void MTLRootLayer::PrepareAttachments(HWDrawContext *context) {
  GPUTextureDescriptor desc{};
  desc.width = color_texture_.width;
  desc.height = color_texture_.height;
  desc.format = GetColorFormat();
  desc.usage = static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);
  desc.sample_count = 1;
  desc.storage_mode = GPUTextureStorageMode::kPrivate;

  color_attachment_ = GPUExternalTextureMTL::Make(desc, color_texture_);
}

void MTLRootLayer::PrepareRenderPassDesc(HWDrawContext *context) {
  HWRenderPassBuilder builder(context, color_attachment_);

  builder.SetSampleCount(GetSampleCount())
      .SetDrawState(GetLayerDrawState())
      .SetLoadOp(NeedClearSurface() ? GPULoadOp::kClear : GPULoadOp::kLoad)
      .SetStoreOp(GPUStoreOp::kStore)
      .Build(render_pass_desc_);
}

std::shared_ptr<GPURenderPass> MTLRootLayer::OnBeginRenderPass(GPUCommandBuffer *cmd) {
  return cmd->BeginRenderPass(render_pass_desc_);
}

}  // namespace skity
