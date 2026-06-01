// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/vk/vk_root_layer.hpp"

#include "src/gpu/gpu_texture.hpp"
#include "src/render/hw/hw_render_pass_builder.hpp"

namespace skity {

namespace {

bool HasTextureUsage(const GPUTextureDescriptor& desc, GPUTextureUsage usage) {
  return (desc.usage & static_cast<GPUTextureUsageMask>(usage)) != 0;
}

}  // namespace

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
    GPUCommandBuffer* cmd, bool force_load) {
  if (force_load && GetSampleCount() == 1) {
    render_pass_desc_.color_attachment.load_op = GPULoadOp::kLoad;
  }

  return cmd->BeginRenderPass(render_pass_desc_);
}

bool VKRootLayer::OnCopyToDstTexture(GPUCommandBuffer* cmd,
                                     std::shared_ptr<GPUTexture> dst_texture,
                                     GPURegion copy_region) const {
  return CopyRegionToDstTexture(cmd, color_attachment_, std::move(dst_texture),
                                copy_region);
}

bool VKRootLayer::SupportsTextureCopyDstRead() const {
  if (color_attachment_ == nullptr) {
    return false;
  }

  const auto& desc = color_attachment_->GetDescriptor();
  if (!HasTextureUsage(desc, GPUTextureUsage::kCopySrc)) {
    return false;
  }

  // Texture-copy dst read itself only needs CopySrc, but MSAA split passes also
  // run an emulated-load draw that samples the single-sample resolve target.
  return GetSampleCount() == 1 ||
         HasTextureUsage(desc, GPUTextureUsage::kTextureBinding);
}

}  // namespace skity
