// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_text_fragment.hpp"

#include "src/gpu/gpu_context_impl.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLTextFragment::WGSLTextFragment(BatchedTexture textures,
                                   std::shared_ptr<GPUSampler> sampler)
    : textures_(std::move(textures)), sampler_(std::move(sampler)) {}

void WGSLTextFragment::PrepareCMD(Command* cmd, HWDrawContext* context) {
  SKITY_TRACE_EVENT(WGSLTextFragment_PrepareCMD);

  // binding all the textures and samplers

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  if (textures_[0] == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);

  if (group == nullptr) {
    return;
  }

  // bind sampler
  {
    auto entry = group->GetEntry(0);
    if (entry == nullptr || entry->type != wgx::BindingType::kSampler) {
      return;
    }

    UploadBindGroup(entry, cmd, sampler_);
  }

  // bind texture
  {
    std::shared_ptr<GPUTexture> last_texture = nullptr;
    for (size_t i = 0; i < textures_.size(); i++) {
      auto entry = group->GetEntry(i + 1);

      if (entry == nullptr || entry->type != wgx::BindingType::kTexture) {
        return;
      }

      std::shared_ptr<GPUTexture> texture = nullptr;
      if (textures_[i] != nullptr) {
        texture = textures_[i];
        last_texture = textures_[i];
      } else {
        texture = last_texture;
      }

      if (texture == nullptr) {
        return;
      }

      UploadBindGroup(entry, cmd, texture);
    }
  }
}

std::string WGSLColorTextFragment::GetShaderName() const {
  std::string name = "ColorTextFragmentWGSL";

  if (filter_ != nullptr) {
    name += "_" + filter_->GetShaderName();
  }

  return name;
}

uint32_t WGSLColorTextFragment::NextBindingIndex() const { return 6; }

std::string WGSLColorTextFragment::GenSourceWGSL() const {
  std::string wgsl_code = kCommonTextFragment;

  if (filter_ != nullptr) {
    wgsl_code += filter_->GenSourceWGSL();
  }

  wgsl_code += R"(
    @group(1) @binding(5) var<uniform> uColor: vec4<f32>;

    struct ColorTextFSInput {
      @location(0) @interpolate(flat) txt_index : i32,
      @location(1)                    v_uv      : vec2<f32>
    };

    @fragment
    fn fs_main(vs_in : ColorTextFSInput) -> @location(0) vec4<f32> {
      var fontAlpha: f32 = get_texture_color(vs_in.txt_index, vs_in.v_uv).r;

      var color: vec4<f32> = vec4<f32>(uColor.rgb * uColor.a, uColor.a);
  )";

  if (filter_ != nullptr) {
    wgsl_code += R"(
      color = filter_color(color);
    )";
  }

  wgsl_code += R"(
      return color * fontAlpha;
    }
  )";

  return wgsl_code;
}

void WGSLColorTextFragment::PrepareCMD(Command* cmd, HWDrawContext* context) {
  WGSLTextFragment::PrepareCMD(cmd, context);

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);

  auto entry = group->GetEntry(5);

  if (entry == nullptr || entry->type_definition->name != "vec4<f32>") {
    return;
  }

  entry->type_definition->SetData(&color_, sizeof(Color4f));

  UploadBindGroup(entry, cmd, context);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

std::string WGSLColorEmojiFragment::GetShaderName() const {
  std::string name = "ColorEmoji";

  if (swizzle_rb_) {
    name += "SwizzleRB";
  } else {
    name += "NoSwizzle";
  }
  name += "FragmentWGSL";

  return name;
}

uint32_t WGSLColorEmojiFragment::NextBindingIndex() const { return 6; }

void WGSLColorEmojiFragment::PrepareCMD(Command* cmd, HWDrawContext* context) {
  WGSLTextFragment::PrepareCMD(cmd, context);

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);

  auto entry = group->GetEntry(5);

  if (entry == nullptr || entry->type_definition->name != "f32") {
    return;
  }

  entry->type_definition->SetData(&alpha_, sizeof(float));

  UploadBindGroup(entry, cmd, context);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

std::string WGSLColorEmojiFragment::GenSourceWGSL() const {
  std::string wgsl_code = kCommonTextFragment;

  wgsl_code += R"(
    @group(1) @binding(5) var<uniform> uAlpha: f32;

    struct ColorTextFSInput {
      @location(0) @interpolate(flat) txt_index : i32,
      @location(1)                    v_uv      : vec2<f32>
    };
  )";

  if (swizzle_rb_) {
    wgsl_code += R"(
      @fragment
      fn fs_main(fs_in : ColorTextFSInput) -> @location(0) vec4<f32> {
        var fontColor: vec4<f32> = get_texture_color(fs_in.txt_index, fs_in.v_uv);

        return vec4<f32>(fontColor.b, fontColor.g, fontColor.r, fontColor.a) * uAlpha;
      }
    )";
  } else {
    wgsl_code += R"(
      @fragment
      fn fs_main(fs_in : ColorTextFSInput) -> @location(0) vec4<f32> {
        var fontColor: vec4<f32> = get_texture_color(fs_in.txt_index, fs_in.v_uv);
        return fontColor * uAlpha;
      }
    )";
  }

  return wgsl_code;
}

uint32_t WGSLGradientTextFragment::NextBindingIndex() const { return 7; }

std::string WGSLGradientTextFragment::GetShaderName() const {
  std::string name = gradient_fragment_.GetShaderName();

  if (filter_ != nullptr) {
    name += "_" + filter_->GetShaderName();
  }

  name += "TextWGSL";

  return name;
}

std::string WGSLGradientTextFragment::GenSourceWGSL() const {
  std::string wgsl_code = kCommonTextFragment;

  wgsl_code += gradient_fragment_.GenSourceWGSL(5);

  wgsl_code += R"(
    struct GradientTextFSInput {
      @location(0) @interpolate(flat) txt_index : i32,
      @location(1)                    v_uv      : vec2<f32>,
      @location(2)                    v_pos     : vec2<f32>,
    };
  )";

  if (type_ == Shader::GradientType::kLinear) {
    wgsl_code += R"(
      @group(1) @binding(6) var<uniform> uLinearInfo    : vec4<f32>;

      fn gradient_text_color(fs_in : GradientTextFSInput) -> vec4<f32> {
        var cs        : vec2<f32> = fs_in.v_pos - uLinearInfo.xy;
        var se        : vec2<f32> = uLinearInfo.zw - uLinearInfo.xy;
        var t         : f32       = dot(cs, se) / dot(se, se);
        var color     : vec4<f32> = calculate_gradient_color(t);

        return vec4<f32>(color.rgb * color.a, color.a) * gradient_info.global_alpha;
      }
    )";
  } else if (type_ == Shader::GradientType::kRadial) {
    wgsl_code += R"(
      @group(1) @binding(6) var<uniform> uRadialInfo    : vec3<f32>;

      fn gradient_text_color(fs_in : GradientTextFSInput) -> vec4<f32> {
        var mixValue  : vec2<f32> = fs_in.v_pos - uRadialInfo.xy;
        var radius    : f32       = uRadialInfo.z;
        var t         : f32       = mixValue / radius;
        var color     : vec4<f32> = calculate_gradient_color(t);

        return vec4<f32>(color.rgb * color.a, color.a) * gradient_info.global_alpha;
      }
    )";

  } else if (type_ == Shader::GradientType::kConical) {
    wgsl_code += R"(
      @group(1) @binding(6) var<uniform> uConicalInfo   : ConicalInfo;

      fn gradient_text_color(fs_in : GradientTextFSInput) -> vec4<f32> {
        var res   : vec2<f32> = calculate_conical_t(fs_in.v_pos,
                                                    uConicalInfo.center1,
                                                    uConicalInfo.center2,
                                                    uConicalInfo.radius1,
                                                    uConicalInfo.radius2);

        if res.y <= 0.0 {
          return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        } else {
          var color     : vec4<f32> = calculate_gradient_color(res.x);

          color.xyz *= color.w;

          return color * gradient_info.global_alpha;
        }
      }
    )";
  }

  if (filter_) {
    wgsl_code += filter_->GenSourceWGSL();
  }

  wgsl_code += R"(
    @fragment
    fn fs_main(fs_in : GradientTextFSInput) -> @location(0) vec4<f32> {
      var fontColor: vec4<f32> = gradient_text_color(fs_in);
      var fontAlpha : f32 = get_texture_color(fs_in.txt_index, fs_in.v_uv).r;
  )";

  if (filter_) {
    wgsl_code += R"(
      fontColor = filter_color(fontColor);
    )";
  }

  wgsl_code += R"(
      return fontColor * fontAlpha;
    }
  )";

  return wgsl_code;
}

void WGSLGradientTextFragment::PrepareCMD(Command* cmd,
                                          HWDrawContext* context) {
  SKITY_TRACE_EVENT(WGSLGradientTextFragment_PrepareCMD);

  WGSLTextFragment::PrepareCMD(cmd, context);

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);

  if (group == nullptr) {
    return;
  }

  auto entry = group->GetEntry(5);

  if (!gradient_fragment_.SetupCommonInfo(entry, global_alpha_)) {
    return;
  }

  UploadBindGroup(entry, cmd, context);

  entry = group->GetEntry(6);
  if (entry == nullptr) {
    return;
  }

  if (!gradient_fragment_.SetupGradientInfo(entry)) {
    return;
  }

  UploadBindGroup(entry, cmd, context);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

/// SDF fragment shader
void WGSLSdfColorTextFragment::PrepareCMD(Command* cmd,
                                          HWDrawContext* context) {
  WGSLTextFragment::PrepareCMD(cmd, context);

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);

  auto entry = group->GetEntry(5);

  if (entry == nullptr || entry->type_definition->name != "vec4<f32>") {
    return;
  }

  entry->type_definition->SetData(&color_, sizeof(Color4f));

  UploadBindGroup(entry, cmd, context);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

std::string WGSLSdfColorTextFragment::GetShaderName() const {
  std::string name = "SdfColorTextFragmentWGSL";

  if (filter_ != nullptr) {
    name += "_" + filter_->GetShaderName();
  }

  return name;
}

uint32_t WGSLSdfColorTextFragment::NextBindingIndex() const { return 6; }

std::string WGSLSdfColorTextFragment::GenSourceWGSL() const {
  std::string wgsl_code = kCommonTextFragment;

  if (filter_ != nullptr) {
    wgsl_code += filter_->GenSourceWGSL();
  }

  // fragment shader comes from skia
  wgsl_code += R"(
    @group(1) @binding(5) var<uniform> uColor: vec4<f32>;

    struct ColorTextFSInput {
      @location(0) @interpolate(flat) txt_index : i32,
      @location(1)                    v_uv      : vec2<f32>
    };

    @fragment
    fn fs_main(vs_in : ColorTextFSInput) -> @location(0) vec4<f32> {
      var distance: f32 = get_texture_color(vs_in.txt_index, vs_in.v_uv).r;
      distance = 7.96875 * (distance - 0.5019608);

      var dist_grad:vec2<f32> = vec2<f32>(dFdx(distance), dFdy(distance));
      let dg_len2: f32 = dot(dist_grad, dist_grad);
      if(dg_len2 < 0.0001) {
        dist_grad = vec2<f32>(0.7071);
      } else {
        dist_grad = dist_grad * inversesqrt(dg_len2);
      }
      let jacobian: mat2x2<f32> = mat2x2<f32>(dFdx(vs_in.v_uv), dFdy(vs_in.v_uv));
      let grad: vec2<f32> = jacobian * dist_grad;
      let afwidth: f32 = 0.65 * length(grad);

      var text_alpha: f32 = smoothstep(-afwidth, afwidth, distance);

      var color: vec4<f32> = vec4<f32>(uColor.rgb * uColor.a, uColor.a);
  )";

  if (filter_ != nullptr) {
    wgsl_code += R"(
      color = filter_color(color);
    )";
  }

  wgsl_code += R"(
      return color * text_alpha;
    }
  )";

  return wgsl_code;
}

}  // namespace skity
