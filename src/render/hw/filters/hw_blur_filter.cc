// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/filters/hw_blur_filter.hpp"

#include "src/render/hw/draw/fragment/wgsl_blur_filter.hpp"
#include "src/render/hw/draw/geometry/wgsl_filter_geometry.hpp"
#include "src/render/hw/draw/step/color_step.hpp"

namespace skity {

HWFilterOutput HWBlurFilter::DoFilter(const HWFilterContext &context,
                                      GPUCommandBuffer *command_buffer) {
  auto draw_context = context.draw_context;
  auto child_output = GetChildOutput(0, context, command_buffer);

  Vec2 input_texture_size = Vec2{child_output.texture->GetDescriptor().width,
                                 child_output.texture->GetDescriptor().height};
  std::shared_ptr<GPUTexture> input_texture = child_output.texture;
  auto transformed_radius = radius_ * context.scale * direction_;
  auto output_texture_size = input_texture_size + Vec2{2} * transformed_radius;
  auto output_texture = CreateOutputTexture(
      input_texture->GetDescriptor().format, output_texture_size, context);
  auto alpha = transformed_radius / output_texture_size;
  // scale = 1 / (1 - 2 * a)
  auto uv_scale = Vec2{1} / (Vec2{1} - 2.0f * alpha);
  // offset = -a / (1 - 2 * a)
  auto uv_offset = -alpha / (Vec2{1} - 2.0f * alpha);
  Command *command = draw_context->arena_allocator->Make<Command>();

  auto render_pass =
      command_buffer->BeginRenderPass(CreateRenderPassDesc(output_texture));
  float radius =
      transformed_radius.x > 0 ? transformed_radius.x : transformed_radius.y;

  PrepareWGXCMD(command, draw_context, input_texture, output_texture,
                direction_, radius, uv_scale, uv_offset);

  Vec2 expand = direction_ * radius_;
  auto layer_bounds =
      Rect::MakeLTRB(child_output.layer_bounds.Left() - expand.x,
                     child_output.layer_bounds.Top() - expand.y,
                     child_output.layer_bounds.Right() + expand.x,
                     child_output.layer_bounds.Bottom() + expand.y);

  render_pass->AddCommand(command);
  render_pass->EncodeCommands();

  return HWFilterOutput{
      output_texture,
      layer_bounds,
  };
}

void HWBlurFilter::PrepareWGXCMD(
    Command *cmd, HWDrawContext *context,
    const std::shared_ptr<GPUTexture> &texture,
    const std::shared_ptr<GPUTexture> &output_texture, const Vec2 &dir,
    float radius, Vec2 uv_scale, Vec2 uv_offset) {
  ColorStep step{context->arena_allocator->Make<WGSLFilterGeometry>(1.0f, 1.0f),
                 context->arena_allocator->Make<WGSLBlurFilter>(
                     texture, dir, radius, uv_scale, uv_offset),
                 CoverageType::kNone};

  HWDrawStepContext step_context{
      context,
      {},
      {},
      0.1f,  // just a little bit bigger than 0
      {
          0,
          0,
          static_cast<float>(output_texture->GetDescriptor().width),
          static_cast<float>(output_texture->GetDescriptor().height),
      },
      output_texture->GetDescriptor().format,
      1,
      BlendMode::kDefault,
      context->scale,
  };

  step.GenerateCommand(step_context, cmd, nullptr);
}

}  // namespace skity
