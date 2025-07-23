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

}  // namespace

WGSLPathGeometry::WGSLPathGeometry(const Path& path, const Paint& paint,
                                   bool is_stroke, bool contour_aa)
    : path_(path),
      paint_(paint),
      is_stroke_(is_stroke),
      contour_aa_(contour_aa),
      layout_(InitVertexBufferLayout(contour_aa_)) {}

const std::vector<GPUVertexBufferLayout>& WGSLPathGeometry::GetBufferLayout()
    const {
  return layout_;
}

std::string WGSLPathGeometry::GenSourceWGSL() const {
  std::string wgsl_code = CommonVertexWGSL();

  if (contour_aa_) {
    wgsl_code += R"(
      struct ContourAAVSInput {
          @location(0)  a_pos     :   vec2<f32>,
          @location(1)  a_pos_aa  :   f32,
      };

      struct ContourAAVSOutput {
          @builtin(position)  v_pos     :   vec4<f32>,
          @location(0)        v_pos_aa  :   f32,
      };

      @group(0) @binding(0) var<uniform> common_slot  : CommonSlot;
      @vertex
      fn vs_main(input : ContourAAVSInput) -> ContourAAVSOutput {
          var output: ContourAAVSOutput;

          output.v_pos    = get_vertex_position(input.a_pos, common_slot);
          output.v_pos_aa = input.a_pos_aa;

          return output;
      }
    )";
  } else {
    wgsl_code += R"(
      @group(0) @binding(0) var<uniform> common_slot: CommonSlot;

      @vertex
      fn vs_main(@location(0) pos: vec3<f32>) -> @builtin(position) vec4<f32> {
          return get_vertex_position(pos.xy, common_slot);
      }
    )";
  }

  return wgsl_code;
}

std::string WGSLPathGeometry::GetShaderName() const {
  std::string name = "CommonPathVertexWGSL";

  if (contour_aa_) {
    name += "AA";
  }

  return name;
}

const char* WGSLPathGeometry::GetEntryPoint() const { return "vs_main"; }

void WGSLPathGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                  const Matrix& transform, float clip_depth,
                                  Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLPathGeometry_PrepareCMD);

  // check the stencil cmd to determine if this is inside a coverage step
  // but this may be changed when implement draw call mergeing in dynamic shader
  // pipeline.
  if (stencil_cmd && !contour_aa_) {
    cmd->index_buffer = stencil_cmd->index_buffer;
    cmd->vertex_buffer = stencil_cmd->vertex_buffer;
    cmd->index_count = stencil_cmd->index_count;
    cmd->uniform_bindings = stencil_cmd->uniform_bindings.Clone();

    return;
  }

  if (cmd->pipeline == nullptr) {
    return;
  }

  auto upload_data = [&](const std::vector<float>& vertex,
                         const std::vector<uint32_t>& index) {
    if (vertex.empty() || index.empty()) {
      return;
    }

    cmd->vertex_buffer = context->stageBuffer->Push(
        const_cast<float*>(vertex.data()), vertex.size() * sizeof(float));

    cmd->index_buffer = context->stageBuffer->PushIndex(
        const_cast<uint32_t*>(index.data()), index.size() * sizeof(uint32_t));

    cmd->index_count = index.size();
  };

  if (is_stroke_) {
    HWPathStrokeRaster raster{paint_, transform, context->vertex_vector_cache,
                              context->index_vector_cache};

    raster.StrokePath(path_);

    upload_data(raster.GetRawVertexBuffer(), raster.GetRawIndexBuffer());
  } else {
    if (contour_aa_) {
      HWPathAAOutline raster{transform, context->vertex_vector_cache,
                             context->index_vector_cache, context->ctx_scale};
      raster.StrokeAAOutline(path_);
      upload_data(raster.GetRawVertexBuffer(), raster.GetRawIndexBuffer());

    } else {
      HWPathFillRaster raster{paint_, transform, context->vertex_vector_cache,
                              context->index_vector_cache};

      raster.FillPath(path_);
      upload_data(raster.GetRawVertexBuffer(), raster.GetRawIndexBuffer());
    }
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

}  // namespace skity
