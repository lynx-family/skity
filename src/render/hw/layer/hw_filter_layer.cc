// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/layer/hw_filter_layer.hpp"

#include "src/gpu/gpu_context_impl.hpp"
#include "src/logging.hpp"
#include "src/render/hw/hw_render_pass_builder.hpp"

namespace skity {

HWFilterLayer::HWFilterLayer(Matrix matrix, int32_t depth, Rect bounds,
                             uint32_t width, uint32_t height,
                             std::shared_ptr<HWFilter> filter)
    : HWSubLayer(matrix, depth, bounds, width, height), filter_(filter) {}

HWDrawState HWFilterLayer::OnPrepare(HWDrawContext* context) {
  auto desc = GetColorTextureDesc();
  auto device = context->gpuContext->GetGPUDevice();
  auto input_texture = device->CreateTexture(desc);

  // If the filter input texture cannot be created, discard this layer
  // instead of running the filter against a null attachment.
  if (!input_texture) {
    LOGE(
        "HWFilterLayer::OnPrepare: failed to allocate filter input texture, "
        "discarding layer");
    return HWDrawState::kDrawStateError;
  }

  HWFilterOutput filter_result{
      input_texture,
      GetBounds(),
  };

  HWFilterContext filter_context{
      device, context->gpuContext, context, filter_result, GetScale(),
  };

  filter_result = filter_->Prepare(filter_context);
  filted_bounds_ = filter_result.layer_bounds;
  SetTextures(input_texture, filter_result.texture);
  auto state = HWSubLayer::OnPrepare(context);
  return state;
}

void HWFilterLayer::OnGenerateCommand(HWDrawContext* context,
                                      HWDrawState state) {
  HWSubLayer::OnGenerateCommand(context, state);
}

void HWFilterLayer::OnPostDraw(GPURenderPass* render_pass,
                               GPUCommandBuffer* cmd) {
  filter_->Filter(cmd);

  HWSubLayer::OnPostDraw(render_pass, cmd);
}

// Children draw into the filter input texture; the filtered output texture only
// holds pixels after the filter runs at post-draw time. Destination reads and
// emulated MSAA loads inside this layer must therefore sample the input
// texture, otherwise advanced blends lose the already-drawn backdrop.
bool HWFilterLayer::OnCopyToDstTexture(GPUCommandBuffer* cmd,
                                       std::shared_ptr<GPUTexture> dst_texture,
                                       GPURegion copy_region) const {
  return CopyRegionToDstTexture(cmd, GetColorTexture(), std::move(dst_texture),
                                copy_region);
}

std::shared_ptr<GPUTexture> HWFilterLayer::GetResolveColorTexture() const {
  return GetColorTexture();
}

}  // namespace skity
