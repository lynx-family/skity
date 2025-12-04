// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/web/web_root_layer.hpp"

#include "src/gpu/web/gpu_texture_web.hpp"
#include "src/render/hw/hw_render_pass_builder.hpp"

namespace skity {

WebRootLayer::WebRootLayer(uint32_t width, uint32_t height, const Rect &bounds,
                           GPUTextureFormat format, WGPUTexture texture)
    : HWRootLayer(width, height, bounds, format), texture_(texture) {}

WebRootLayer::~WebRootLayer() = default;

HWDrawState WebRootLayer::OnPrepare(HWDrawContext *context) {
  PrepareAttachments(context);

  auto ret = HWRootLayer::OnPrepare(context);

  PrepareRenderPassDesc(context);

  return ret;
}

void WebRootLayer::PrepareAttachments(HWDrawContext *context) {
  GPUTextureDescriptor desc{};
  desc.width = wgpuTextureGetWidth(texture_);
  desc.height = wgpuTextureGetHeight(texture_);
  desc.format = GetColorFormat();
  desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);
  desc.sample_count = 1;
  desc.storage_mode = GPUTextureStorageMode::kPrivate;

  wgpuTextureAddRef(
      texture_);  // release will be called when GPUTextureWEB delete
  color_attachment_ = std::make_shared<GPUTextureWEB>(desc, nullptr, texture_);
}

void WebRootLayer::PrepareRenderPassDesc(HWDrawContext *context) {
  HWRenderPassBuilder builder(context, color_attachment_);

  builder.SetSampleCount(GetSampleCount())
      .SetDrawState(GetLayerDrawState())
      .SetLoadOp(NeedClearSurface() ? GPULoadOp::kClear : GPULoadOp::kLoad)
      .SetStoreOp(GPUStoreOp::kStore)
      .Build(render_pass_desc_);
}

std::shared_ptr<GPURenderPass> WebRootLayer::OnBeginRenderPass(
    GPUCommandBuffer *cmd) {
  return cmd->BeginRenderPass(render_pass_desc_);
}

}  // namespace skity
