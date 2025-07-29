// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/filters/hw_down_sampler_filter.hpp"

#include <cstring>

#include "src/render/hw/draw/fragment/wgsl_image_filter.hpp"
#include "src/render/hw/draw/geometry/wgsl_filter_geometry.hpp"
#include "src/render/hw/draw/step/color_step.hpp"

namespace skity {

HWDownSamplerFilter::HWDownSamplerFilter(std::shared_ptr<HWFilter> input,
                                         float scale)
    : HWFilter({input}), scale_(scale) {}

HWFilterOutput HWDownSamplerFilter::DoFilter(const HWFilterContext &context,
                                             GPUCommandBuffer *command_buffer) {
  auto draw_context = context.draw_context;

  auto child_output = GetChildOutput(0, context, command_buffer);

  Vec2 input_texture_size = Vec2{child_output.texture->GetDescriptor().width,
                                 child_output.texture->GetDescriptor().height};
  std::shared_ptr<GPUTexture> input_texture = child_output.texture;

  auto output_texture_size = input_texture_size * scale_;

  uint32_t width = glm::ceil(std::abs(output_texture_size.x));
  uint32_t height = glm::ceil(std::abs(output_texture_size.y));

  if (width == 0 || height == 0) {
    // skip downsampler filter if output size is zero
    return child_output;
  }

  auto output_texture = CreateOutputTexture(
      input_texture->GetDescriptor().format, output_texture_size, context);

  auto render_pass =
      command_buffer->BeginRenderPass(CreateRenderPassDesc(output_texture));

  Command *command = context.draw_context->arena_allocator->Make<Command>();

  PrepareCMDWGX(draw_context, command, input_texture, output_texture_size);

  render_pass->AddCommand(command);
  render_pass->EncodeCommands();

  return HWFilterOutput{
      output_texture,
      child_output.layer_bounds,
  };
}

void HWDownSamplerFilter::PrepareCMDWGX(
    HWDrawContext *context, Command *cmd,
    const std::shared_ptr<GPUTexture> &input_texture, const Vec2 &output_size) {
  auto geometry =
      context->arena_allocator->Make<WGSLFilterGeometry>(1.0f, 1.0f);
  auto fragment =
      context->arena_allocator->Make<WGSLImageFilter>(input_texture);

  ColorStep step{
      geometry,
      fragment,
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
          output_size.x,
          output_size.y,
      },
      input_texture->GetDescriptor().format,
      1,
      BlendMode::kDefault,
  };

  step.GenerateCommand(step_context, cmd, nullptr);
}

}  // namespace skity
