// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/filters/hw_merge_filter.hpp"

namespace skity {

HWFilterOutput HWMergeFilter::DoFilter(const HWFilterContext& context,
                                       GPUCommandBuffer* command_buffer) {
  if (GetChildCount() == 0) {
    return context.source;
  }

  Rect layer_bounds = Rect::MakeEmpty();
  std::vector<HWFilterOutput> children_outputs;
  for (size_t i = 0; i < GetChildCount(); i++) {
    children_outputs.push_back(GetChildOutput(i, context, command_buffer));
  }
  // Calculate layer bounds;
  for (auto child_output : children_outputs) {
    layer_bounds.Join(child_output.layer_bounds);
  }

  Vec2 output_texture_size = glm::abs(glm::round(
      Vec2{layer_bounds.Width(), layer_bounds.Height()} * context.scale));
  auto color_format = children_outputs[0].texture->GetDescriptor().format;
  auto output_texture =
      CreateOutputTexture(color_format, output_texture_size, context);
  auto render_pass_desc = CreateRenderPassDesc(output_texture);
  auto render_pass = command_buffer->BeginRenderPass(render_pass_desc);
  DrawChildrenOutputs(context, render_pass.get(), output_texture_size,
                      color_format, layer_bounds, children_outputs);
  render_pass->EncodeCommands();
  return HWFilterOutput{.texture = output_texture,
                        .layer_bounds = layer_bounds};
}

}  // namespace skity
