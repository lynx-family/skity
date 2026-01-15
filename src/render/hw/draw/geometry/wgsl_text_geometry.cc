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
                  GPUVertexFormat::kFloat32x4,
                  0,
                  0,
              },
          },
      },
      GPUVertexBufferLayout{
          12 * sizeof(float),
          GPUVertexStepMode::kInstance,
          {
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  0,
                  1,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  4 * sizeof(float),
                  2,
              },
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x4,
                  8 * sizeof(float),
                  3,
              },
          },
      },
  };

  return layout;
}

struct Instance {
  Vec4 vertex_coord;
  Vec4 texture_coord;
  Vec4 color;
};

static_assert(sizeof(Instance) == 48);

void WGSLTextGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                  const Matrix& transform, float clip_depth,
                                  Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLTextGeometry_PrepareCMD);

  if (cmd->pipeline == nullptr) {
    return;
  }

  cmd->vertex_buffer = context->static_buffer->GetTextVertexBufferView();
  cmd->index_buffer = context->static_buffer->GetTextIndexBufferView();
  cmd->index_count = cmd->index_buffer.range / sizeof(uint32_t);

  context->stageBuffer->BeginWritingInstance(
      glyph_rects_.size() * sizeof(Instance), alignof(Instance));
  for (auto& glyph_rect : glyph_rects_) {
    context->stageBuffer->AppendInstance<Instance>(
        glyph_rect.item.vertex_coord,
        Vec4{glyph_rect.item.texture_coord_tl,
             glyph_rect.item.texture_coord_br},
        glyph_rect.paint.GetColor4f());
  }
  auto instance_buffer_view = context->stageBuffer->EndWritingInstance();
  cmd->instance_count = instance_buffer_view.range / sizeof(Instance);
  cmd->instance_buffer = instance_buffer_view;

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

  UploadBindGroup(group->group, common_slot, cmd, context);
}

bool WGSLTextGeometry::CanMerge(const HWWGSLGeometry* other) const {
  return GetMainKey() == other->GetMainKey();
}
void WGSLTextGeometry::Merge(const HWWGSLGeometry* other) {
  auto o = static_cast<const WGSLTextGeometry*>(other);
  for (auto&& glyph_rect : o->glyph_rects_) {
    glyph_rects_.emplace_back(std::move(glyph_rect));
  }
}

namespace {

struct Vertex {
  Vec4 vertex_offset;
};

static_assert(sizeof(Vertex) == 16);

}  // namespace

GPUBufferView WGSLTextGeometry::CreateVertexBufferView(
    HWStageBuffer* stage_bufer) {
  auto vertex_array = std::array<Vertex, 4>{
      // Top Left
      Vertex{Vec4{1, 1, 0, 0}},
      // Bottom Left
      Vertex{Vec4{1, 0, 0, 1}},
      // Top Right
      Vertex{Vec4{0, 1, 1, 0}},
      // Bottom Right
      Vertex{Vec4{0, 0, 1, 1}},
  };

  return stage_bufer->Push(reinterpret_cast<float*>(vertex_array.data()),
                           vertex_array.size() * sizeof(Vertex));
}

GPUBufferView WGSLTextGeometry::CreateIndexBufferView(
    HWStageBuffer* stage_bufer) {
  auto index_array = std::array<uint32_t, 6>{
      0, 1, 2, 1, 3, 2,
  };
  return stage_bufer->PushIndex(const_cast<uint32_t*>(index_array.data()),
                                index_array.size() * sizeof(uint32_t));
}

std::string WGSLTextSolidColorGeometry::GenSourceWGSL() const {
  std::string wgsl_code = CommonVertexWGSL();

  wgsl_code += kTextCommonVertex;

  wgsl_code += R"(
    struct TextSolidColorVSOutput {
        @builtin(position)              pos         : vec4<f32>,
        @location(0) @interpolate(flat) txt_index   : i32,
        @location(1)                    v_uv        : vec2<f32>,
        @location(2)                    v_color     : vec4<f32>
    };

    @vertex
    fn vs_main(text_in: TextVSInput) -> TextSolidColorVSOutput {
        var output: TextSolidColorVSOutput;
        var pos: vec2<f32> = vec2<f32>(text_in.a_offset.x * text_in.a_pos.x + text_in.a_offset.z * text_in.a_pos.z,
                                       text_in.a_offset.y * text_in.a_pos.y + text_in.a_offset.w * text_in.a_pos.w);
        var uv: vec2<f32> = vec2<f32>(text_in.a_offset.x * text_in.a_uv.x + text_in.a_offset.z * text_in.a_uv.z,
                                       text_in.a_offset.y * text_in.a_uv.y + text_in.a_offset.w * text_in.a_uv.w);

        output.pos          = get_vertex_position(pos, common_slot);
        output.txt_index    = get_texture_index(uv.x);
        output.v_uv         = get_texture_uv(uv);
        output.v_color      = text_in.a_color;
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

        var pos: vec2<f32> = vec2<f32>(text_in.a_offset.x * text_in.a_pos.x + text_in.a_offset.z * text_in.a_pos.z,
                                       text_in.a_offset.y * text_in.a_pos.y + text_in.a_offset.w * text_in.a_pos.w);
        var uv: vec2<f32> = vec2<f32>(text_in.a_offset.x * text_in.a_uv.x + text_in.a_offset.z * text_in.a_uv.z,
                                       text_in.a_offset.y * text_in.a_uv.y + text_in.a_offset.w * text_in.a_uv.w);

        output.pos          = get_vertex_position(pos, common_slot);
        output.txt_index    = get_texture_index(uv.x);
        output.v_uv         = get_texture_uv(uv);
        output.v_pos        = (inv_matrix * common_slot.userTransform * vec4<f32>(pos, 0.0, 1.0)).xy;

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

  UploadBindGroup(group->group, entry, cmd, context);
}

bool WGSLTextGradientGeometry::CanMerge(const HWWGSLGeometry* other) const {
  return false;
}

}  // namespace skity
