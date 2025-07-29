// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/filters/hw_color_filter.hpp"

#include "src/render/hw/draw/fragment/wgsl_image_filter.hpp"
#include "src/render/hw/draw/geometry/wgsl_filter_geometry.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/wgx_filter.hpp"

namespace skity {
HWFilterOutput HWColorFilter::DoFilter(const HWFilterContext &context,
                                       GPUCommandBuffer *command_buffer) {
  auto draw_context = context.draw_context;

  auto child_output = GetChildOutput(0, context, command_buffer);

  Vec2 input_texture_size = Vec2{child_output.texture->GetDescriptor().width,
                                 child_output.texture->GetDescriptor().height};
  std::shared_ptr<GPUTexture> input_texture = child_output.texture;

  auto output_texture_size = input_texture_size;
  auto output_texture = CreateOutputTexture(
      input_texture->GetDescriptor().format, output_texture_size, context);

  Command *command = context.draw_context->arena_allocator->Make<Command>();

  auto render_pass =
      command_buffer->BeginRenderPass(CreateRenderPassDesc(output_texture));

  PrepareCMD(draw_context, command, input_texture);

  render_pass->AddCommand(command);
  render_pass->EncodeCommands();

  return HWFilterOutput{
      output_texture,
      child_output.layer_bounds,
  };
}

void HWColorFilter::PrepareCMD(
    HWDrawContext *draw_context, Command *cmd,
    const std::shared_ptr<GPUTexture> &input_texture) {
  InternalPrepareCMDWGX(draw_context, cmd, input_texture);
}

void HWColorFilter::InternalPrepareCMDWGX(
    HWDrawContext *context, Command *cmd,
    const std::shared_ptr<GPUTexture> &input_texture) {
  auto fragment =
      context->arena_allocator->Make<WGSLImageFilter>(input_texture);

  fragment->SetFilter(WGXFilterFragment::Make(color_filter_.get()));

  ColorStep step{
      context->arena_allocator->Make<WGSLFilterGeometry>(1.0f, 1.0f),
      std::move(fragment),
      CoverageType::kNone,
  };

  HWDrawStepContext step_context{
      context,
      {},
      {},
      0.1f,  // just a little bit bigger than 0
      {
          0,
          0,
          static_cast<float>(input_texture->GetDescriptor().width),
          static_cast<float>(input_texture->GetDescriptor().height),
      },
      input_texture->GetDescriptor().format,
      1,
      BlendMode::kDefault,
  };

  step.GenerateCommand(step_context, cmd, nullptr);
}

}  // namespace skity
