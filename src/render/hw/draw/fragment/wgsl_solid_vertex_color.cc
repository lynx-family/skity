// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_solid_vertex_color.hpp"

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLSolidVertexColor::WGSLSolidVertexColor()
    : HWWGSLFragment(Flags::kSnippet | Flags::kAffectsVertex) {}

uint32_t WGSLSolidVertexColor::NextBindingIndex() const { return 1; }

HWFunctionBaseKey WGSLSolidVertexColor::GetMainKey() const {
  return HWFragmentKeyType::kSolidVertex;
}

std::optional<std::vector<std::string>> WGSLSolidVertexColor::GetVarings()
    const {
  return std::vector<std::string>{"f_color: vec4<f32>"};
}

void WGSLSolidVertexColor::WriteVSAssgnShadingVarings(
    std::stringstream& ss) const {
  ss << R"( 
  output.f_color = input.color;
)";
}

void WGSLSolidVertexColor::WriteFSMain(std::stringstream& ss) const {
  ss << R"(  
  color = vec4<f32>(input.f_color.rgb * input.f_color.a, input.f_color.a);
)";
}

void WGSLSolidVertexColor::PrepareCMD(Command* cmd, HWDrawContext* context) {
  SKITY_TRACE_EVENT(WGSLSolidVertexColor_PrepareCMD);

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

}  // namespace skity
