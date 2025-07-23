// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_gradient_fragment.hpp"

#include "src/render/hw/hw_draw.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLGradientFragment::WGSLGradientFragment(Shader::GradientInfo info,
                                           Shader::GradientType type,
                                           float global_alpha)
    : info_(info),
      type_(type),
      global_alpha_(global_alpha),
      gradient_fragment_(info_, type_) {}

std::string WGSLGradientFragment::GetShaderName() const {
  std::string name = gradient_fragment_.GetShaderName();

  if (filter_ != nullptr) {
    name += "_" + filter_->GetShaderName();
  }

  if (contour_aa_) {
    name += "_AA";
  }

  name += "PathWGSL";

  return name;
}

uint32_t WGSLGradientFragment::NextBindingIndex() const { return 2; }

std::string WGSLGradientFragment::GenSourceWGSL() const {
  std::string wgsl = gradient_fragment_.GenSourceWGSL(0);

  if (type_ == Shader::GradientType::kLinear) {
    wgsl += R"(
        @group(1) @binding(1) var<uniform> linear_pts       : vec4<f32>;

        fn generate_gradient_color(v_pos: vec2<f32>) -> vec4<f32> {
            var cs: vec2<f32> = v_pos - linear_pts.xy;
            var se: vec2<f32> = linear_pts.zw - linear_pts.xy;

            var t: f32 = dot(cs, se) / dot(se, se);

            var color: vec4<f32> = calculate_gradient_color(t);

            color.xyz *= color.w;

            return color * gradient_info.global_alpha;
        }
    )";
  } else if (type_ == Shader::GradientType::kRadial) {
    wgsl += R"(
        @group(1) @binding(1) var<uniform> radial_pts       : vec3<f32>;

        fn generate_gradient_color(v_pos: vec2<f32>) -> vec4<f32> {
            var mixValue: f32 = distance(v_pos, radial_pts.xy);
            var radius: f32 = radial_pts.z;
            var t: f32 = mixValue / radius;

            var color: vec4<f32> = calculate_gradient_color(t);

            color.xyz *= color.w;

            return color * gradient_info.global_alpha;
        }
    )";
  } else if (type_ == Shader::GradientType::kConical) {
    wgsl += R"(
      @group(1) @binding(1) var<uniform> conical_info     : ConicalInfo;

      fn generate_gradient_color(v_pos: vec2<f32>) -> vec4<f32> {
        var res: vec2<f32> = calculate_conical_t(v_pos, conical_info.center1, conical_info.center2, conical_info.radius1, conical_info.radius2);

        if res.y <= 0.0 {
          return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        } else {
          var color: vec4<f32> = calculate_gradient_color(res.x);

          color.xyz *= color.w;

          return color * gradient_info.global_alpha;
        }
      }
    )";
  } else if (type_ == Shader::GradientType::kSweep) {
    wgsl += R"(
      @group(1) @binding(1) var<uniform> sweep_pts       : vec4<f32>;

      const k1Over2Pi: f32 = 0.1591549430918;

      fn generate_gradient_color(v_pos: vec2<f32>) -> vec4<f32> {
        var center: vec2<f32> = sweep_pts.xy;
        var bias: f32 = sweep_pts.z;
        var scale: f32 = sweep_pts.w;
        var cood: vec2<f32> = v_pos - center;
        var angle: f32 = atan(-cood.y, -cood.x);
        var t: f32 = (angle * k1Over2Pi + 0.5 + bias) * scale;

        var color: vec4<f32> = calculate_gradient_color(t);

        color.xyz *= color.w;

        return color * gradient_info.global_alpha;
      }
    )";
  }

  if (filter_ != nullptr) {
    wgsl += filter_->GenSourceWGSL();
  }

  if (contour_aa_) {
    wgsl += R"(
      struct GradientFSAAInput {
        @location(0)  v_pos    : vec2<f32>,
        @location(1)  v_pos_aa : f32,
      };

      @fragment
      fn fs_main(input: GradientFSAAInput) -> @location(0) vec4<f32> {
        var color: vec4<f32> = generate_gradient_color(input.v_pos);
    )";
  } else {
    wgsl += R"(
      @fragment
      fn fs_main(@location(0) v_pos: vec2<f32>) -> @location(0) vec4<f32> {
        var color: vec4<f32> = generate_gradient_color(v_pos);
    )";
  }

  if (filter_ != nullptr) {
    wgsl += R"(
      color = filter_color(color);
    )";
  }

  if (contour_aa_) {
    wgsl += R"(
       color *= input.v_pos_aa;
    )";
  }

  wgsl += R"(
        return color;
    }
  )";

  return wgsl;
}

const char* WGSLGradientFragment::GetEntryPoint() const { return "fs_main"; }

void WGSLGradientFragment::PrepareCMD(Command* cmd, HWDrawContext* context) {
  SKITY_TRACE_EVENT(WGSLGradientFragment_PrepareCMD);

  if (cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);

  if (group == nullptr) {
    return;
  }

  auto gradient_info_entry = group->GetEntry(0);

  if (gradient_info_entry == nullptr) {
    return;
  }

  if (!gradient_fragment_.SetupCommonInfo(gradient_info_entry, global_alpha_)) {
    return;
  }

  UploadBindGroup(gradient_info_entry, cmd, context);

  auto gradient_type_entry = group->GetEntry(1);

  if (gradient_type_entry == nullptr) {
    return;
  }

  if (!gradient_fragment_.SetupGradientInfo(gradient_type_entry)) {
    return;
  }

  UploadBindGroup(gradient_type_entry, cmd, context);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

}  // namespace skity
