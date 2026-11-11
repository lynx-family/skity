// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/geometry/wgsl_rect_geometry.hpp"

#include <cstddef>

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

static_assert(sizeof(WGSLRectGeometry::Instance) == 48);

WGSLRectGeometry::WGSLRectGeometry(
    const Instance& first_instance,
    const std::vector<Instance>& additional_instances)
    : HWWGSLGeometry(Flags::kSnippet),
      first_instance_(first_instance),
      additional_instances_(additional_instances) {}

std::vector<GPUVertexBufferLayout> WGSLRectGeometry::GetBufferLayout() {
  return {
      {sizeof(Vec4),
       GPUVertexStepMode::kVertex,
       {{GPUVertexFormat::kFloat32x4, 0, 0}}},
      {sizeof(Instance),
       GPUVertexStepMode::kInstance,
       {{GPUVertexFormat::kFloat32x4, offsetof(Instance, rect), 1},
        {GPUVertexFormat::kFloat32x4, offsetof(Instance, scale_translate), 2},
        {GPUVertexFormat::kFloat32x4, offsetof(Instance, color), 3}}},
  };
}

void WGSLRectGeometry::WriteVSFunctionsAndStructs(std::stringstream& ss) const {
  ss << CommonVertexWGSL();
}

void WGSLRectGeometry::WriteVSUniforms(std::stringstream& ss) const {
  ss << "@group(0) @binding(0) var<uniform> common_slot: CommonSlot;\n";
}

void WGSLRectGeometry::WriteVSInput(std::stringstream& ss) const {
  ss << R"(
struct VSInput {
  @location(0) corner: vec4<f32>,
  @location(1) rect: vec4<f32>,
  @location(2) scale_translate: vec4<f32>,
  @location(3) color: vec4<f32>,
};
)";
}

void WGSLRectGeometry::WriteVSMain(std::stringstream& ss) const {
  ss << R"(
  local_pos = input.corner.xy * input.rect.xy +
              input.corner.zw * input.rect.zw;
  let t: vec4<f32> = input.scale_translate;
  var rect_slot: CommonSlot = common_slot;
  rect_slot.userTransform = mat4x4<f32>(
      vec4<f32>(t.x, 0.0, 0.0, 0.0),
      vec4<f32>(0.0, t.y, 0.0, 0.0),
      vec4<f32>(0.0, 0.0, 1.0, 0.0),
      vec4<f32>(t.z, t.w, 0.0, 1.0));
  output.pos = get_vertex_position(local_pos, rect_slot);
)";
}

void WGSLRectGeometry::PrepareCMD(Command* cmd, HWDrawContext* context,
                                  const Matrix& transform, float clip_depth,
                                  Command*) {
  SKITY_TRACE_EVENT(WGSLRectGeometry_PrepareCMD);

  if (!UploadCommonInfo(cmd, context, transform, clip_depth)) {
    return;
  }

  cmd->vertex_buffer = context->static_buffer->GetUnitQuadVertexBufferView();
  cmd->index_buffer = context->static_buffer->GetUnitQuadIndexBufferView();
  cmd->index_count = cmd->index_buffer.range / sizeof(uint32_t);

  auto* stage_buffer = context->stageBuffer;
  stage_buffer->BeginWritingInstance(
      (1 + additional_instances_.size()) * sizeof(Instance), alignof(Instance));
  stage_buffer->AppendInstance<Instance>(first_instance_);
  for (const auto& instance : additional_instances_) {
    stage_buffer->AppendInstance<Instance>(instance);
  }
  cmd->instance_buffer = stage_buffer->EndWritingInstance();
  cmd->instance_count = cmd->instance_buffer.range / sizeof(Instance);
}

}  // namespace skity
