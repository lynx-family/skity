// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_solid_color.hpp"

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLSolidColor::WGSLSolidColor(const Color4f& color)
    : HWWGSLFragment(Flags::kSnippet), color_(color) {}

uint32_t WGSLSolidColor::NextBindingIndex() const { return 1; }

std::string WGSLSolidColor::GetShaderName() const { return "SolidColor"; }

void WGSLSolidColor::WriteFSUniforms(std::stringstream& ss) const {
  ss << R"(
@group(1) @binding(0) var<uniform> uColor: vec4<f32>;
)";
}

void WGSLSolidColor::WriteFSMain(std::stringstream& ss) const {
  ss << R"(
  color = vec4<f32>(uColor.rgb * uColor.a, uColor.a);
)";
}

void WGSLSolidColor::PrepareCMD(Command* cmd, HWDrawContext* context) {
  SKITY_TRACE_EVENT(WGSLSolidColor_PrepareCMD);

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);

  if (group == nullptr) {
    return;
  }

  auto color_binding = group->GetEntry(0);
  if (color_binding->type_definition->name != "vec4<f32>") {
    return;
  }

  color_binding->type_definition->SetData(&color_, sizeof(Color4f));

  UploadBindGroup(color_binding, cmd, context);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

}  // namespace skity
