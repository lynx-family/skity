// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/filters/hw_matrix_filter.hpp"

namespace skity {

HWFilterOutput HWMatrixFilter::DoFilter(const HWFilterContext& context,
                                        GPUCommandBuffer* command_buffer) {
  auto child_output = GetChildOutput(0, context, command_buffer);
  Rect layer_bounds;
  matrix_.MapRect(&layer_bounds, child_output.layer_bounds);

  Vec2 output_texture_size = glm::abs(glm::round(
      Vec2{layer_bounds.Width(), layer_bounds.Height()} * context.scale));
  auto color_format = child_output.texture->GetDescriptor().format;
  auto output_texture =
      CreateOutputTexture(color_format, output_texture_size, context);
  auto render_pass_desc = CreateRenderPassDesc(output_texture);
  auto render_pass = command_buffer->BeginRenderPass(render_pass_desc);

  child_output.matrix = matrix_;
  DrawChildrenOutputs(context, render_pass.get(), output_texture_size,
                      color_format, layer_bounds, std::vector{child_output});

  render_pass->EncodeCommands();
  return HWFilterOutput{.texture = output_texture,
                        .layer_bounds = layer_bounds};
}

}  // namespace skity
