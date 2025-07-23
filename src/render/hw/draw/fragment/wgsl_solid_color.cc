// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_solid_color.hpp"

#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLSolidColor::WGSLSolidColor(const Color4f& color) : color_(color) {}

uint32_t WGSLSolidColor::NextBindingIndex() const { return 1; }

std::string WGSLSolidColor::GetShaderName() const {
  std::string name = "SolidColorFragmentWGSL";

  if (filter_) {
    name += "_" + filter_->GetShaderName();
  }

  if (contour_aa_) {
    name += "AA";
  }

  return name;
}

std::string WGSLSolidColor::GenSourceWGSL() const {
  std::string wgsl_code = R"(
    @group(1) @binding(0) var<uniform> uColor: vec4<f32>;
  )";

  if (filter_ != nullptr) {
    wgsl_code += filter_->GenSourceWGSL();
  }

  if (contour_aa_) {
    wgsl_code += R"(
      @fragment
      fn fs_main(@location(0) v_pos_aa: f32) -> @location(0) vec4<f32> {
    )";
  } else {
    wgsl_code += R"(
    @fragment
    fn fs_main() -> @location(0) vec4<f32> {
    )";
  }

  wgsl_code += R"(
    var color : vec4<f32> = vec4<f32>(uColor.rgb * uColor.a, uColor.a);
  )";

  if (filter_ != nullptr) {
    wgsl_code += R"(
       color = filter_color(color);
    )";
  }

  if (contour_aa_) {
    wgsl_code += R"(
        return color * v_pos_aa;
      }
    )";
  } else {
    wgsl_code += R"(
        return color;
      }
    )";
  }

  return wgsl_code;
}

const char* WGSLSolidColor::GetEntryPoint() const { return "fs_main"; }

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
