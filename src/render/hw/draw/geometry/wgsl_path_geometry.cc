// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_path_geometry.hpp"

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_path_aa_outline.hpp"
#include "src/render/hw/hw_path_raster.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

namespace {

std::vector<GPUVertexBufferLayout> InitVertexBufferLayout(bool aa) {
  std::vector<GPUVertexBufferLayout> layout = {
      GPUVertexBufferLayout{
          3 * sizeof(float),
          GPUVertexStepMode::kVertex,
          {
              GPUVertexAttribute{
                  GPUVertexFormat::kFloat32x2,
                  0,
                  0,
              },
          },
      },
  };

  if (aa) {
    layout.front().attributes.emplace_back(GPUVertexAttribute{
        GPUVertexFormat::kFloat32,
        2 * sizeof(float),
        1,
    });
  }

  return layout;
}

void UploadData(Command* cmd, HWDrawContext* context,
                const std::vector<float>& vertex,
                const std::vector<uint32_t>& index) {
  if (vertex.empty() || index.empty()) {
    return;
  }

  cmd->vertex_buffer = context->stageBuffer->Push(
      const_cast<float*>(vertex.data()), vertex.size() * sizeof(float));

  cmd->index_buffer = context->stageBuffer->PushIndex(
      const_cast<uint32_t*>(index.data()), index.size() * sizeof(uint32_t));

  cmd->index_count = index.size();
}

}  // namespace

WGSLPathGeometry::WGSLPathGeometry(const Path& path, const Paint& paint,
                                   bool is_stroke)
    : HWWGSLGeometry(Flags::kSnippet),
      path_(path),
      paint_(paint),
      is_stroke_(is_stroke),
      layout_(InitVertexBufferLayout(false)) {}

const std::vector<GPUVertexBufferLayout>& WGSLPathGeometry::GetBufferLayout()
    const {
  return layout_;
}

void WGSLPathGeometry::WriteVSFunctionsAndStructs(std::stringstream& ss) const {
  ss << CommonVertexWGSL();
}

void WGSLPathGeometry::WriteVSUniforms(std::stringstream& ss) const {
  ss << "@group(0) @binding(0) var<uniform> common_slot  : CommonSlot;\n";
}

void WGSLPathGeometry::WriteVSInput(std::stringstream& ss) const {
  ss << R"(
struct VSInput {
  @location(0)  a_pos: vec2<f32>,
};
)";
}

void WGSLPathGeometry::WriteVSMain(std::stringstream& ss) const {
  ss << R"(
  local_pos = input.a_pos;
  output.pos = get_vertex_position(input.a_pos, common_slot);
)";
}

std::string WGSLPathGeometry::GetShaderName() const { return "Path"; }

void WGSLPathGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                  const Matrix& transform, float clip_depth,
                                  Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLPathGeometry_PrepareCMD);

  // check the stencil cmd to determine if this is inside a coverage step
  // but this may be changed when implement draw call mergeing in dynamic shader
  // pipeline.
  if (stencil_cmd) {
    cmd->index_buffer = stencil_cmd->index_buffer;
    cmd->vertex_buffer = stencil_cmd->vertex_buffer;
    cmd->index_count = stencil_cmd->index_count;
    cmd->uniform_bindings = stencil_cmd->uniform_bindings.Clone();
    return;
  }

  if (cmd->pipeline == nullptr) {
    return;
  }

  const Vec2& scale = context->scale;

  if (is_stroke_) {
    HWPathStrokeRaster raster{
        paint_, Matrix::Scale(scale.x, scale.y) * transform,
        context->vertex_vector_cache, context->index_vector_cache};

    raster.StrokePath(path_);

    UploadData(cmd, context, raster.GetRawVertexBuffer(),
               raster.GetRawIndexBuffer());
  } else {
    HWPathFillRaster raster{paint_, Matrix::Scale(scale.x, scale.y) * transform,
                            context->vertex_vector_cache,
                            context->index_vector_cache};

    raster.FillPath(path_);
    UploadData(cmd, context, raster.GetRawVertexBuffer(),
               raster.GetRawIndexBuffer());
  }

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

WGSLPathAAGeometry::WGSLPathAAGeometry(const Path& path, const Paint& paint)
    : HWWGSLGeometry(Flags::kSnippet | Flags::kAffectsFragment),
      path_(path),
      paint_(paint),
      layout_(InitVertexBufferLayout(true)) {}

const std::vector<GPUVertexBufferLayout>& WGSLPathAAGeometry::GetBufferLayout()
    const {
  return layout_;
}

void WGSLPathAAGeometry::WriteVSFunctionsAndStructs(
    std::stringstream& ss) const {
  ss << CommonVertexWGSL();
}

void WGSLPathAAGeometry::WriteVSUniforms(std::stringstream& ss) const {
  ss << "@group(0) @binding(0) var<uniform> common_slot  : CommonSlot;\n";
}

void WGSLPathAAGeometry::WriteVSInput(std::stringstream& ss) const {
  ss << R"(
struct VSInput {
  @location(0)  a_pos: vec2<f32>,
  @location(1)  a_pos_aa: f32,
};
)";
}

void WGSLPathAAGeometry::WriteVSMain(std::stringstream& ss) const {
  ss << R"(
  local_pos = input.a_pos;
  output.pos = get_vertex_position(input.a_pos, common_slot);
  output.v_pos_aa = input.a_pos_aa;
)";
}

std::string WGSLPathAAGeometry::GetShaderName() const { return "PathAA"; }

void WGSLPathAAGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                    const Matrix& transform, float clip_depth,
                                    Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLPathGeometry_PrepareCMD);
  if (cmd->pipeline == nullptr) {
    return;
  }

  const Vec2& scale = context->scale;

  HWPathAAOutline raster{Matrix::Scale(scale.x, scale.y) * transform,
                         context->vertex_vector_cache,
                         context->index_vector_cache, context->ctx_scale};
  raster.StrokeAAOutline(path_);
  UploadData(cmd, context, raster.GetRawVertexBuffer(),
             raster.GetRawIndexBuffer());

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

std::optional<std::vector<std::string>> WGSLPathAAGeometry::GetVarings() const {
  return std::vector<std::string>{"v_pos_aa: f32"};
}

std::string WGSLPathAAGeometry::GetFSNameSuffix() const { return "AA"; }

void WGSLPathAAGeometry::WriteFSAlphaMask(std::stringstream& ss) const {
  ss << R"(
  mask_alpha = input.v_pos_aa;
)";
}

}  // namespace skity
