// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/layer/hw_filter_layer.hpp"

#include "src/gpu/gpu_context_impl.hpp"
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
  auto command_buffer =
      std::make_shared<GPUCommandBufferProxy>(device->CreateCommandBuffer());

  HWFilterOutput filter_result{
      input_texture,
      GetBounds(),
  };

  HWFilterContext filter_context{
      device,        context->gpuContext, context,
      filter_result, command_buffer,      GetScale(),
  };

  filter_result = filter_->Filter(filter_context);
  command_buffer_ = filter_context.command_buffer;
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
  command_buffer_->Submit();
  HWSubLayer::OnPostDraw(render_pass, cmd);
}

}  // namespace skity
