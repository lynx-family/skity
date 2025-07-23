
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_filter_geometry.hpp"

#include <array>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLFilterGeometry::WGSLFilterGeometry(float u_factor, float v_factor)
    : u_factor_(u_factor), v_factor_(v_factor), vertex_buffer_() {
  // left top
  vertex_buffer_[0] = -1.f;
  vertex_buffer_[1] = 1.f;
  // left bottom
  vertex_buffer_[2] = -1.f;
  vertex_buffer_[3] = -1.f;
  // right top
  vertex_buffer_[4] = 1.f;
  vertex_buffer_[5] = 1.f;
  // right bottom
  vertex_buffer_[6] = 1.f;
  vertex_buffer_[7] = -1.f;
}

WGSLFilterGeometry::WGSLFilterGeometry(
    float u_factor, float v_factor, const std::array<float, 8> &vertex_buffer)
    : u_factor_(u_factor), v_factor_(v_factor), vertex_buffer_(vertex_buffer) {}

const std::vector<GPUVertexBufferLayout> &WGSLFilterGeometry::GetBufferLayout()
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

std::string WGSLFilterGeometry::GetShaderName() const {
  return "CommonFilaterVertexWGSL";
}

std::string WGSLFilterGeometry::GenSourceWGSL() const {
  return R"(
       struct FilterVertInput {
            @location(0) a_pos  : vec2<f32>,
            @location(1) a_uv   : vec2<f32>,
       };

       struct FilterVertOutput {
            @builtin(position) v_pos   : vec4<f32>,
            @location(0)       v_uv    : vec2<f32>,
       };

       @vertex
       fn vs_main(vs_in : FilterVertInput) -> FilterVertOutput {
           var vs_out : FilterVertOutput;

           vs_out.v_pos = vec4<f32>(vs_in.a_pos, 0.0, 1.0);
           vs_out.v_uv  = vs_in.a_uv;

           return vs_out;
       }
    )";
}

const char *WGSLFilterGeometry::GetEntryPoint() const { return "vs_main"; }

void WGSLFilterGeometry::PrepareCMD(Command *cmd, HWDrawContext *context,
                                    const Matrix &transform, float clip_depth,
                                    Command *) {
  SKITY_TRACE_EVENT(WGSLFilterGeometry_PrepareCMD);

  // [x, y, u, v] * 4
  std::array<float, 16> raw_vertex{};

  // left top point
  raw_vertex[0] = vertex_buffer_[0];
  raw_vertex[1] = vertex_buffer_[1];
  raw_vertex[2] = 0.f;
  raw_vertex[3] = 0.f;
  // left bottom point
  raw_vertex[4] = vertex_buffer_[2];
  raw_vertex[5] = vertex_buffer_[3];
  raw_vertex[6] = 0.f;
  raw_vertex[7] = v_factor_;
  // right top point
  raw_vertex[8] = vertex_buffer_[4];
  raw_vertex[9] = vertex_buffer_[5];
  raw_vertex[10] = u_factor_;
  raw_vertex[11] = 0.f;
  // right bottom point
  raw_vertex[12] = vertex_buffer_[6];
  raw_vertex[13] = vertex_buffer_[7];
  raw_vertex[14] = u_factor_;
  raw_vertex[15] = v_factor_;

  // TODO(zhangzhijian): Need to find a way to do the following work without
  // being aware of the backend.
  if (context->gpuContext->GetBackendType() == GPUBackendType::kOpenGL) {
    raw_vertex[3] = 1 - raw_vertex[3];
    raw_vertex[7] = 1 - raw_vertex[7];
    raw_vertex[11] = 1 - raw_vertex[11];
    raw_vertex[15] = 1 - raw_vertex[15];
  }

  std::array<uint32_t, 6> raw_index{};

  raw_index[0] = 0;
  raw_index[1] = 1;
  raw_index[2] = 2;
  raw_index[3] = 2;
  raw_index[4] = 1;
  raw_index[5] = 3;

  cmd->vertex_buffer = context->stageBuffer->Push(
      raw_vertex.data(), raw_vertex.size() * sizeof(float));

  cmd->index_buffer = context->stageBuffer->PushIndex(
      raw_index.data(), raw_index.size() * sizeof(uint32_t));
  cmd->index_count = 6;
}

}  // namespace skity
