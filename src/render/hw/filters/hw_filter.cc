// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/filters/hw_filter.hpp"

#include <cstring>

#include "src/render/hw/draw/fragment/wgsl_image_filter.hpp"
#include "src/render/hw/draw/geometry/wgsl_filter_geometry.hpp"
#include "src/render/hw/draw/step/color_step.hpp"

namespace skity {
HWFilterOutput HWFilter::Filter(HWFilterContext& context) {
  auto result = DoFilter(context, context.command_buffer.get());
  return result;
}

std::shared_ptr<GPUTexture> HWFilter::CreateOutputTexture(
    GPUTextureFormat format, Vec2 output_texture_size,
    const HWFilterContext& context) {
  GPUTextureDescriptor texture_desc{};

  texture_desc.usage =
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding) |
      static_cast<GPUTextureUsageMask>(GPUTextureUsage::kRenderAttachment);
  texture_desc.format = format;
  texture_desc.width = glm::ceil(output_texture_size.x);
  texture_desc.height = glm::ceil(output_texture_size.y);
  texture_desc.storage_mode = GPUTextureStorageMode::kPrivate;
  auto output_texture = context.device->CreateTexture(texture_desc);
  return output_texture;
}

GPURenderPassDescriptor HWFilter::CreateRenderPassDesc(
    std::shared_ptr<GPUTexture> output_texture) {
  GPURenderPassDescriptor desc;
  desc.color_attachment.texture = output_texture;
  desc.color_attachment.load_op = GPULoadOp::kClear;
  desc.color_attachment.store_op = GPUStoreOp::kStore;
  desc.color_attachment.clear_value = GPUColor{0.0, 0.0, 0.0, 0.0};
  return desc;
}

HWFilterOutput HWFilter::GetChildOutput(size_t index,
                                        const HWFilterContext& context,
                                        GPUCommandBuffer* command_buffer) {
  if (inputs_.size() <= index) {
    return context.source;
  }

  auto child_filter = inputs_[index];
  if (child_filter) {
    return child_filter->DoFilter(context, command_buffer);
  }
  return context.source;
}

void HWFilter::DrawChildrenOutputs(
    const HWFilterContext& context, GPURenderPass* render_pass,
    Vec2 output_texture_size, GPUTextureFormat color_format,
    const Rect& layer_bounds,
    const std::vector<HWFilterOutput>& children_outputs) {
  InternalDrawChildrenOutpusWGX(context, render_pass, output_texture_size,
                                color_format, layer_bounds, children_outputs);
}

void HWFilter::InternalDrawChildrenOutpusWGX(
    const HWFilterContext& context, GPURenderPass* render_pass,
    Vec2 output_texture_size, GPUTextureFormat color_format,
    const Rect& layer_bounds,
    const std::vector<HWFilterOutput>& children_outputs) {
  AutoSetMVP auto_mvp{context.draw_context, layer_bounds};

  for (const auto& output : children_outputs) {
    auto matrix = context.draw_context->mvp * output.matrix;

    std::array<Vec2, 4> vertex_pos = {
        Vec2{output.layer_bounds.Left(), output.layer_bounds.Top()},
        Vec2{output.layer_bounds.Left(), output.layer_bounds.Bottom()},
        Vec2{output.layer_bounds.Right(), output.layer_bounds.Top()},
        Vec2{output.layer_bounds.Right(), output.layer_bounds.Bottom()},
    };

    std::array<Vec2, 4> mapped_vertex_pos = {};
    matrix.MapPoints(mapped_vertex_pos.data(), vertex_pos.data(), 4);

    std::array<float, 8> raw_vertex = {};

    std::memcpy(raw_vertex.data(), mapped_vertex_pos.data(), sizeof(Vec2) * 4);

    ColorStep step{
        context.draw_context->arena_allocator->Make<WGSLFilterGeometry>(
            1.f, 1.f, raw_vertex),
        context.draw_context->arena_allocator->Make<WGSLImageFilter>(
            output.texture),
        CoverageType::kNone,
    };

    HWDrawStepContext step_context{
        context.draw_context,
        {},
        {},
        0.1f,  // just a little bit bigger than 0
        {
            0,
            0,
            output_texture_size.x,
            output_texture_size.y,
        },
        color_format,
        1,
        BlendMode::kDefault,
        context.scale,
    };

    auto cmd = context.draw_context->arena_allocator->Make<Command>();
    step.GenerateCommand(step_context, cmd, nullptr);
    render_pass->AddCommand(cmd);
  }
}

}  // namespace skity
