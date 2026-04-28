// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/vk/vk_root_layer.hpp"

#include "src/render/hw/hw_render_pass_builder.hpp"

namespace skity {

VKRootLayer::VKRootLayer(uint32_t width, uint32_t height,
                         std::shared_ptr<GPUTexture> texture,
                         const Rect& bounds, GPUTextureFormat format)
    : HWRootLayer(width, height, bounds, format),
      color_attachment_(std::move(texture)) {}

HWDrawState VKRootLayer::OnPrepare(HWDrawContext* context) {
  auto state = HWRootLayer::OnPrepare(context);

  if (color_attachment_ == nullptr) {
    return state;
  }

  HWRenderPassBuilder builder(context, color_attachment_);
  builder.SetSampleCount(GetSampleCount())
      .SetDrawState(GetLayerDrawState())
      .SetLoadOp(NeedClearSurface() ? GPULoadOp::kClear : GPULoadOp::kLoad)
      .SetStoreOp(GPUStoreOp::kStore)
      .Build(render_pass_desc_);

  return state;
}

std::shared_ptr<GPURenderPass> VKRootLayer::OnBeginRenderPass(
    GPUCommandBuffer* cmd) {
  return cmd->BeginRenderPass(render_pass_desc_);
}

}  // namespace skity
