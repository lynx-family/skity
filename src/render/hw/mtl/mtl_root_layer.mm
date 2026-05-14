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
  desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment) |
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
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

  render_pass_desc_.label = "MTLRootLayer";
}

std::shared_ptr<GPURenderPass> MTLRootLayer::OnBeginRenderPass(
    GPUCommandBuffer *cmd, bool force_load) {
  (void)force_load;
  if (!first_render_pass_) {
    render_pass_desc_.color_attachment.load_op = GPULoadOp::kLoad;
  }
  first_render_pass_ = false;
  return cmd->BeginRenderPass(render_pass_desc_);
}

bool MTLRootLayer::OnCopyToDstTexture(GPUCommandBuffer *cmd,
                                      std::shared_ptr<GPUTexture> dst_texture,
                                      GPURegion copy_region) const {
  // assert(dst_texture && copy_region.width > 0 && copy_region.height > 0);
  auto blit_pass = cmd->BeginBlitPass();
  blit_pass->CopyTextureToTexture(color_attachment_, dst_texture,
                                  GPUBlitPass::TextureCopyRegion{
                                      .src_x = copy_region.x,
                                      .src_y = copy_region.y,
                                      .dst_x = 0,
                                      .dst_y = 0,
                                      .width = copy_region.width,
                                      .height = copy_region.height,
                                  });
  blit_pass->End();
  return true;
}

}  // namespace skity
