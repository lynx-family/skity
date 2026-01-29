// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/filters/hw_matrix_filter.hpp"

namespace skity {

HWFilterOutput HWMatrixFilter::Prepare(const HWFilterContext& context) {
  auto child_output = GetChildOutput(0, context);
  Rect layer_bounds;
  matrix_.MapRect(&layer_bounds, child_output.layer_bounds);

  Vec2 output_texture_size = Vec2::Abs(Vec2::Round(
      Vec2{layer_bounds.Width(), layer_bounds.Height()} * context.scale));
  auto color_format = child_output.texture->GetDescriptor().format;
  auto output_texture =
      CreateOutputTexture(color_format, output_texture_size, context);

  SetOutputTexture(output_texture);

  auto cmd = context.draw_context->arena_allocator->Make<Command>();

  std::vector<Command*> commands = {cmd};

  child_output.matrix = matrix_;
  DrawChildrenOutputs(context, commands, output_texture_size, color_format,
                      layer_bounds, std::vector{child_output});

  AddCommand(cmd);

  return HWFilterOutput{
      output_texture,
      layer_bounds,
  };
}

}  // namespace skity
