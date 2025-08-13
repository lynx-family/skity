// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_path_raster.hpp"
#include "src/tracing.hpp"

namespace skity {

const std::vector<GPUVertexBufferLayout>& WGSLTextGeometry::GetBufferLayout()
    const {
  static const std::vector<GPUVertexBufferLayout> layout = {
      GPUVertexBufferLayout{
          4 * sizeof(float),
          GPUVertexStepMode::kVertex,
          {
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x2,
                  0,
                  0,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x2,
                  2 * sizeof(float),
                  1,
              },
          },
      },
  };

  return layout;
}

void WGSLTextGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                  const Matrix& transform, float clip_depth,
                                  Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLTextGeometry_PrepareCMD);

  if (cmd->pipeline == nullptr) {
    return;
  }

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);
  HWPathFillRaster raster{paint, transform, context->vertex_vector_cache,
                          context->index_vector_cache};

  for (auto& glyph_rect : glyph_rects_) {
    raster.FillTextRect(glyph_rect.vertex_coord, glyph_rect.texture_coord_tl,
                        glyph_rect.texture_coord_br);
  }

  const auto& vertex = raster.GetRawVertexBuffer();
  const auto& index = raster.GetRawIndexBuffer();

  cmd->vertex_buffer = context->stageBuffer->Push(
      const_cast<float*>(vertex.data()), vertex.size() * sizeof(float));
  cmd->index_buffer = context->stageBuffer->PushIndex(
      const_cast<uint32_t*>(index.data()), index.size() * sizeof(uint32_t));

  cmd->index_count = index.size();

  auto pipeline = cmd->pipeline;

  auto group = pipeline->GetBindingGroup(0);
  if (group == nullptr) {
    return;
  }

  // bind CommonSlot
  auto common_slot = group->GetEntry(0);

  if (!SetupCommonInfo(common_slot, context->mvp, transform, clip_depth)) {
    return;
  }

  UploadBindGroup(common_slot, cmd, context);
}

std::string WGSLTextSolidColorGeometry::GenSourceWGSL() const {
  std::string wgsl_code = CommonVertexWGSL();

  wgsl_code += kTextCommonVertex;

  wgsl_code += R"(
    struct TextSolidColorVSOutput {
        @builtin(position)              pos         : vec4<f32>,
        @location(0) @interpolate(flat) txt_index   : i32,
        @location(1)                    v_uv        : vec2<f32>,
    };

    @vertex
    fn vs_main(text_in: TextVSInput) -> TextSolidColorVSOutput {
        var output: TextSolidColorVSOutput;

        output.pos          = get_vertex_position(text_in.a_pos, common_slot);
        output.txt_index    = get_texture_index(text_in.a_uv.x);
        output.v_uv         = get_texture_uv(text_in.a_uv);
        return output;
    }
  )";

  return wgsl_code;
}

std::string WGSLTextGradientGeometry::GenSourceWGSL() const {
  std::string wgsl_code = CommonVertexWGSL();

  wgsl_code += kTextCommonVertex;

  wgsl_code += R"(
    struct TextGradientVSOutput {
        @builtin(position)              pos         : vec4<f32>,
        @location(0) @interpolate(flat) txt_index   : i32,
        @location(1)                    v_uv        : vec2<f32>,
        @location(2)                    v_pos       : vec2<f32>,
    };

    @group(0) @binding(1) var<uniform> inv_matrix   : mat4x4<f32>;

    @vertex
    fn vs_main(text_in: TextVSInput) -> TextGradientVSOutput {
        var output: TextGradientVSOutput;

        output.pos          = get_vertex_position(text_in.a_pos, common_slot);
        output.txt_index    = get_texture_index(text_in.a_uv.x);
        output.v_uv         = get_texture_uv(text_in.a_uv);
        output.v_pos        = (inv_matrix * common_slot.userTransform * vec4<f32>(text_in.a_pos, 0.0, 1.0)).xy;

        return output;
    }
  )";

  return wgsl_code;
}

void WGSLTextGradientGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                          const Matrix& transform,
                                          float clip_depth,
                                          Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLTextGradientGeometry_PrepareCMD);
  WGSLTextGeometry::PrepareCMD(cmd, context, transform, clip_depth,
                               stencil_cmd);

  auto pipeline = cmd->pipeline;

  if (pipeline == nullptr) {
    return;
  }

  auto group = pipeline->GetBindingGroup(0);

  if (group == nullptr) {
    return;
  }

  auto entry = group->GetEntry(1);

  if (entry == nullptr || entry->type_definition == nullptr ||
      entry->type_definition->name != "mat4x4<f32>") {
    return;
  }

  entry->type_definition->SetData(&inv_matrix_, sizeof(Matrix));

  UploadBindGroup(entry, cmd, context);
}

}  // namespace skity
