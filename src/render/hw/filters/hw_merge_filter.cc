// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/filters/hw_merge_filter.hpp"

namespace skity {

HWFilterOutput HWMergeFilter::Prepare(const HWFilterContext& context) {
  if (GetChildCount() == 0) {
    return context.source;
  }

  Rect layer_bounds = Rect::MakeEmpty();
  std::vector<HWFilterOutput> children_outputs;
  for (size_t i = 0; i < GetChildCount(); i++) {
    children_outputs.push_back(GetChildOutput(i, context));
  }
  // Calculate layer bounds;
  for (auto child_output : children_outputs) {
    layer_bounds.Join(child_output.layer_bounds);
  }

  Vec2 output_texture_size = Vec2::Abs(Vec2::Round(
      Vec2{layer_bounds.Width(), layer_bounds.Height()} * context.scale));
  auto color_format = children_outputs[0].texture->GetDescriptor().format;
  auto output_texture =
      CreateOutputTexture(color_format, output_texture_size, context);

  SetOutputTexture(output_texture);

  std::vector<Command*> commands;

  for (size_t i = 0; i < children_outputs.size(); i++) {
    commands.emplace_back(
        context.draw_context->arena_allocator->Make<Command>());

    AddCommand(commands.back());
  }

  DrawChildrenOutputs(context, commands, output_texture_size, color_format,
                      layer_bounds, children_outputs);

  return HWFilterOutput{
      output_texture,
      layer_bounds,
  };
}

}  // namespace skity
