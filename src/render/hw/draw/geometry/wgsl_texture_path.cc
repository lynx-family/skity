// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_texture_path.hpp"

#include <array>

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLTexturePath::WGSLTexturePath(const Path& path, const Paint& paint,
                                 bool is_stroke, bool contour_aa,
                                 const Matrix& local_matrix, float width,
                                 float height)
    : WGSLPathGeometry(path, paint, is_stroke, contour_aa),
      local_matrix_(local_matrix),
      width_(width),
      height_(height) {}

std::string WGSLTexturePath::GenSourceWGSL() const {
  std::string wgsl_code = CommonVertexWGSL();

  if (IsContourAA()) {
    wgsl_code += R"(
      struct ImageAAVSInput {
        @location(0)  a_pos     :   vec2<f32>,
        @location(1)  a_pos_aa  :   f32,
      };

      struct ImageVSOutput {
        @builtin(position)  position    : vec4<f32>,
        @location(0)        frag_coord  : vec2<f32>,
        @location(1)        v_pos_aa    : f32,
      };
    )";
  } else {
    wgsl_code += R"(
      struct ImageVSOutput {
        @builtin(position)  position    : vec4<f32>,
        @location(0)        frag_coord  : vec2<f32>,
      };
    )";
  }

  wgsl_code += R"(
    struct ImageBoundsInfo {
      bounds      : vec2<f32>,
      inv_matrix  : mat4x4<f32>,
    };

    @group(0) @binding(0) var<uniform> common_slot  : CommonSlot;
    @group(0) @binding(1) var<uniform> image_bounds : ImageBoundsInfo;
  )";

  if (IsContourAA()) {
    wgsl_code += R"(
       @vertex
       fn vs_main(input: ImageAAVSInput) -> ImageVSOutput {
          var vs_output: ImageVSOutput;
          vs_output.position              = get_vertex_position(input.a_pos, common_slot);
          var mapped_pos  : vec2<f32>     = (image_bounds.inv_matrix * vec4<f32>(input.a_pos, 0.0, 1.0)).xy;
    )";
  } else {
    wgsl_code += R"(
       @vertex
       fn vs_main(@location(0) pos: vec3<f32>) -> ImageVSOutput {
          var vs_output: ImageVSOutput;
          vs_output.position              = get_vertex_position(pos.xy, common_slot);
          var mapped_pos  : vec2<f32>     = (image_bounds.inv_matrix * vec4<f32>(pos.xy, 0.0, 1.0)).xy;
    )";
  }

  wgsl_code += R"(
            var mapped_lt   : vec2<f32>     = vec2<f32>(0.0, 0.0);
            var mapped_rb   : vec2<f32>     = image_bounds.bounds;
            var total_x     : f32           = mapped_rb.x - mapped_lt.x;
            var total_y     : f32           = mapped_rb.y - mapped_lt.y;
            var v_x         : f32           = (mapped_pos.x - mapped_lt.x) / total_x;
            var v_y         : f32           = (mapped_pos.y - mapped_lt.y) / total_y;

            vs_output.frag_coord = vec2<f32>(v_x, v_y);
    )";

  if (IsContourAA()) {
    wgsl_code += R"(
         vs_output.v_pos_aa = input.a_pos_aa;
    )";
  }

  wgsl_code += R"(
       return vs_output;
    }
  )";

  return wgsl_code;
}

std::string WGSLTexturePath::GetShaderName() const {
  std::string name = "ImagePathVertexWGSL";

  if (IsContourAA()) {
    name += "AA";
  }

  return name;
}

const char* WGSLTexturePath::GetEntryPoint() const { return "vs_main"; }

void WGSLTexturePath::PrepareCMD(Command* cmd, HWDrawContext* context,
                                 const Matrix& transform, float clip_depth,
                                 Command* stencil_cmd) {
  SKITY_TRACE_EVENT(WGSLTexturePath_PrepareCMD);

  WGSLPathGeometry::PrepareCMD(cmd, context, transform, clip_depth,
                               stencil_cmd);

  if (cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(0);
  if (group == nullptr) {
    return;
  }

  auto image_bounds_entry = group->GetEntry(1);
  if (image_bounds_entry == nullptr ||
      image_bounds_entry->type_definition->name != "ImageBoundsInfo") {
    return;
  }

  auto image_bounds_struct = static_cast<wgx::StructDefinition*>(
      image_bounds_entry->type_definition.get());

  std::array<float, 2> bounds{width_, height_};
  image_bounds_struct->GetMember("bounds")->type->SetData(
      bounds.data(), bounds.size() * sizeof(float));

  image_bounds_struct->GetMember("inv_matrix")
      ->type->SetData(&local_matrix_, sizeof(Matrix));

  UploadBindGroup(image_bounds_entry, cmd, context);
}

}  // namespace skity
