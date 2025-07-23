// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_gradient_path.hpp"

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLGradientPath::WGSLGradientPath(const Path& path, const Paint& paint,
                                   bool is_stroke, bool contour_aa,
                                   const Matrix& local_matrix)
    : WGSLPathGeometry(path, paint, is_stroke, contour_aa),
      local_matrix_(local_matrix) {}

std::string WGSLGradientPath::GenSourceWGSL() const {
  std::string wgsl_code = CommonVertexWGSL();

  if (IsContourAA()) {
    wgsl_code += R"(
      struct GradientVSOutput {
        @builtin(position)  pos     :   vec4<f32>,
        @location(0)        v_pos   :   vec2<f32>,
        @location(1)        v_pos_aa:   f32,
      };

      struct GracientVSInput {
        @location(0)  a_pos     :   vec2<f32>,
        @location(1)  a_pos_aa  :   f32,
      };
    )";
  } else {
    wgsl_code += R"(
      struct GradientVSOutput {
        @builtin(position)  pos     :   vec4<f32>,
        @location(0)        v_pos   :   vec2<f32>,
      };
    )";
  }

  wgsl_code += R"(
    @group(0) @binding(0) var<uniform> common_slot  : CommonSlot;
    @group(0) @binding(1) var<uniform> inv_matrix   : mat4x4<f32>;
  )";

  if (IsContourAA()) {
    wgsl_code += R"(
      @vertex
      fn vs_main(input: GracientVSInput) -> GradientVSOutput {
          var output: GradientVSOutput;
          output.pos      = get_vertex_position(input.a_pos, common_slot);
          output.v_pos    = (inv_matrix * vec4<f32>(input.a_pos, 0.0, 1.0)).xy;
          output.v_pos_aa = input.a_pos_aa;
          return output;
      }
    )";
  } else {
    wgsl_code += R"(
      @vertex
      fn vs_main(@location(0) pos: vec2<f32>) -> GradientVSOutput {
          var output: GradientVSOutput;
          output.pos = get_vertex_position(pos.xy, common_slot);
          output.v_pos = (inv_matrix * vec4<f32>(pos.xy, 0.0, 1.0)).xy;
          return output;
      }
    )";
  }

  return wgsl_code;
}

std::string WGSLGradientPath::GetShaderName() const {
  std::string name = "CommonGradientPathVertexWGSL";

  if (IsContourAA()) {
    name += "AA";
  }

  return name;
}

const char* WGSLGradientPath::GetEntryPoint() const { return "vs_main"; }

void WGSLGradientPath::PrepareCMD(Command* cmd, HWDrawContext* context,
                                  const Matrix& transform, float clip_depth,
                                  Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLGradientPath_PrepareCMD);

  WGSLPathGeometry::PrepareCMD(cmd, context, transform, clip_depth,
                               stencil_cmd);

  if (cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(0);

  if (group == nullptr) {
    return;
  }

  auto inv_matrix_entry = group->GetEntry(1);

  if (inv_matrix_entry == nullptr ||
      inv_matrix_entry->type_definition->name != "mat4x4<f32>") {
    return;
  }

  Matrix inv_matrix{};

  local_matrix_.Invert(&inv_matrix);

  inv_matrix_entry->type_definition->SetData(&inv_matrix, sizeof(Matrix));

  UploadBindGroup(inv_matrix_entry, cmd, context);
}

}  // namespace skity
