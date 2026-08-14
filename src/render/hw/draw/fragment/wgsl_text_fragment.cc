// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_text_fragment.hpp"

#include "src/gpu/gpu_context_impl.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_pipeline_key.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLTextFragment::WGSLTextFragment(BatchedTexture textures,
                                   std::shared_ptr<GPUSampler> sampler)
    : HWWGSLFragment(Flags::kSnippet),
      textures_(std::move(textures)),
      sampler_(std::move(sampler)) {}

void WGSLTextFragment::WriteFSFunctionsAndStructs(std::stringstream& ss) const {
  ss << kCommonTextFunctions;
}

void WGSLTextFragment::WriteFSUniforms(std::stringstream& ss) const {
  ss << R"(
@group(1) @binding(0) var uSampler      : sampler;
@group(1) @binding(1) var uFontTexture0 : texture_2d<f32>;
@group(1) @binding(2) var uFontTexture1 : texture_2d<f32>;
@group(1) @binding(3) var uFontTexture2 : texture_2d<f32>;
@group(1) @binding(4) var uFontTexture3 : texture_2d<f32>;
)";
}

void WGSLTextFragment::WriteFSCoverage(std::stringstream& ss) const {
  ss << R"(
  coverage = get_texture_color(input.v_txt_index, input.v_uv).r;
)";
}

void WGSLTextFragment::PrepareCMD(Command* cmd, HWDrawContext* context) {
  SKITY_TRACE_EVENT(WGSLTextFragment_PrepareCMD);

  if (cmd == nullptr || cmd->pipeline == nullptr || textures_[0] == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);
  if (group == nullptr) {
    return;
  }

  auto sampler_entry = group->GetEntry(0);
  if (sampler_entry == nullptr ||
      sampler_entry->type != wgx::BindingType::kSampler) {
    return;
  }
  UploadBindGroup(group->group, sampler_entry, cmd, sampler_);

  std::shared_ptr<GPUTexture> last_texture;
  for (size_t i = 0; i < textures_.size(); i++) {
    auto entry = group->GetEntry(i + 1);
    if (entry == nullptr || entry->type != wgx::BindingType::kTexture) {
      return;
    }

    if (textures_[i] != nullptr) {
      last_texture = textures_[i];
    }
    if (last_texture == nullptr) {
      return;
    }
    UploadBindGroup(group->group, entry, cmd, last_texture);
  }
}

bool WGSLTextFragment::CanMerge(const HWWGSLFragment* other) const {
  auto o = static_cast<const WGSLTextFragment*>(other);
  for (size_t i = 0; i < textures_.size(); i++) {
    if (textures_[i] != o->textures_[i]) {
      return false;
    }
  }
  if (sampler_ != o->sampler_) {
    return false;
  }
  if ((filter_ && !o->filter_) || (!filter_ && o->filter_)) {
    return false;
  }
  if (filter_) {
    DEBUG_CHECK(o->filter_ != nullptr);
    if (filter_->GetType() != o->filter_->GetType()) {
      return false;
    }
    if (filter_->GetType() == HWColorFilterKeyType::kCompose) {
      return false;
    }
  }
  return true;
}

HWFunctionBaseKey WGSLColorTextFragment::GetMainKey() const {
  return HWFragmentKeyType::kColorText;
}

uint32_t WGSLColorTextFragment::NextBindingIndex() const { return 6; }

void WGSLColorTextFragment::WriteFSMain(std::stringstream& ss) const {
  ss << R"(
  color = vec4<f32>(input.v_color.rgb * input.v_color.a, input.v_color.a);
)";
}

void WGSLColorTextFragment::PrepareCMD(Command* cmd, HWDrawContext* context) {
  WGSLTextFragment::PrepareCMD(cmd, context);

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

bool WGSLColorTextFragment::CanMerge(const HWWGSLFragment* other) const {
  return GetMainKey() == other->GetMainKey() &&
         WGSLTextFragment::CanMerge(other);
}

HWFunctionBaseKey WGSLColorEmojiFragment::GetMainKey() const {
  return MakeMainKey(HWFragmentKeyType::kEmojiText, swizzle_rb_ ? 1 : 0);
}

uint32_t WGSLColorEmojiFragment::NextBindingIndex() const { return 6; }

void WGSLColorEmojiFragment::WriteFSUniforms(std::stringstream& ss) const {
  WGSLTextFragment::WriteFSUniforms(ss);
  ss << R"(
@group(1) @binding(5) var<uniform> uAlpha: f32;
)";
}

void WGSLColorEmojiFragment::WriteFSMain(std::stringstream& ss) const {
  ss << R"(
  color = get_texture_color(input.v_txt_index, input.v_uv);
)";
  if (swizzle_rb_) {
    ss << "  color = color.bgra;\n";
  }
  ss << "  color = color * uAlpha;\n";
}

void WGSLColorEmojiFragment::PrepareCMD(Command* cmd, HWDrawContext* context) {
  WGSLTextFragment::PrepareCMD(cmd, context);

  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);
  if (group == nullptr) {
    return;
  }
  auto entry = group->GetEntry(5);
  if (entry == nullptr || entry->type_definition == nullptr ||
      entry->type_definition->name != "f32") {
    return;
  }

  entry->type_definition->SetData(&alpha_, sizeof(float));
  UploadBindGroup(group->group, entry, cmd, context);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

bool WGSLColorEmojiFragment::CanMerge(const HWWGSLFragment* other) const {
  if (GetMainKey() != other->GetMainKey() ||
      !WGSLTextFragment::CanMerge(other)) {
    return false;
  }
  auto o = static_cast<const WGSLColorEmojiFragment*>(other);
  return swizzle_rb_ == o->swizzle_rb_ && alpha_ == o->alpha_;
}

uint32_t WGSLGradientTextFragment::NextBindingIndex() const { return 7; }

HWFunctionBaseKey WGSLGradientTextFragment::GetMainKey() const {
  return MakeMainKey(HWFragmentKeyType::kGradientText,
                     gradient_fragment_.GetCustomKey());
}

void WGSLGradientTextFragment::WriteFSFunctionsAndStructs(
    std::stringstream& ss) const {
  WGSLTextFragment::WriteFSFunctionsAndStructs(ss);
  ss << gradient_fragment_.GenSourceWGSL(5);

  if (type_ == Shader::GradientType::kLinear) {
    ss << R"(
fn gradient_text_color(v_pos: vec2<f32>) -> vec4<f32> {
  let cs: vec2<f32> = v_pos - uLinearInfo.xy;
  let se: vec2<f32> = uLinearInfo.zw - uLinearInfo.xy;
  let t: f32 = dot(cs, se) / dot(se, se);
  var color: vec4<f32> = calculate_gradient_color(t);
  if gradient_info.flags == 0 {
    color = vec4<f32>(color.xyz * color.w, color.w);
  }
  return color * gradient_info.global_alpha;
}
)";
  } else if (type_ == Shader::GradientType::kRadial) {
    ss << R"(
fn gradient_text_color(v_pos: vec2<f32>) -> vec4<f32> {
  let t: f32 = distance(v_pos, uRadialInfo.xy) / uRadialInfo.z;
  var color: vec4<f32> = calculate_gradient_color(t);
  if gradient_info.flags == 0 {
    color = vec4<f32>(color.xyz * color.w, color.w);
  }
  return color * gradient_info.global_alpha;
}
)";
  } else if (type_ == Shader::GradientType::kConical) {
    ss << R"(
fn gradient_text_color(v_pos: vec2<f32>) -> vec4<f32> {
  let result: vec2<f32> = calculate_conical_t(
      v_pos, uConicalInfo.center1, uConicalInfo.center2,
      uConicalInfo.radius1, uConicalInfo.radius2);
  if result.y <= 0.0 {
    return vec4<f32>(0.0);
  }
  var color: vec4<f32> = calculate_gradient_color(result.x);
  if gradient_info.flags == 0 {
    color = vec4<f32>(color.xyz * color.w, color.w);
  }
  return color * gradient_info.global_alpha;
}
)";
  } else if (type_ == Shader::GradientType::kSweep) {
    ss << R"(
fn gradient_text_color(v_pos: vec2<f32>) -> vec4<f32> {
  let kOneOverTwoPi: f32 = 0.1591549430918;
  let coord: vec2<f32> = v_pos - uSweepInfo.xy;
  let angle: f32 = atan(-coord.y, -coord.x);
  let t: f32 = (angle * kOneOverTwoPi + 0.5 + uSweepInfo.z) * uSweepInfo.w;
  var color: vec4<f32> = calculate_gradient_color(t);
  if gradient_info.flags == 0 {
    color = vec4<f32>(color.xyz * color.w, color.w);
  }
  return color * gradient_info.global_alpha;
}
)";
  }
}

void WGSLGradientTextFragment::WriteFSUniforms(std::stringstream& ss) const {
  WGSLTextFragment::WriteFSUniforms(ss);
  if (type_ == Shader::GradientType::kLinear) {
    ss << R"(
@group(1) @binding(6) var<uniform> uLinearInfo: vec4<f32>;
)";
  } else if (type_ == Shader::GradientType::kRadial) {
    ss << R"(
@group(1) @binding(6) var<uniform> uRadialInfo: vec3<f32>;
)";
  } else if (type_ == Shader::GradientType::kConical) {
    ss << R"(
@group(1) @binding(6) var<uniform> uConicalInfo: ConicalInfo;
)";
  } else if (type_ == Shader::GradientType::kSweep) {
    ss << R"(
@group(1) @binding(6) var<uniform> uSweepInfo: vec4<f32>;
)";
  }
}

void WGSLGradientTextFragment::WriteFSMain(std::stringstream& ss) const {
  ss << "  color = gradient_text_color(input.v_pos);\n";
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
  UploadBindGroup(group->group, entry, cmd, context);

  entry = group->GetEntry(6);
  if (entry == nullptr || !gradient_fragment_.SetupGradientInfo(entry)) {
    return;
  }
  UploadBindGroup(group->group, entry, cmd, context);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

bool WGSLGradientTextFragment::CanMerge(const HWWGSLFragment* other) const {
  (void)other;
  return false;
}

HWFunctionBaseKey WGSLSdfColorTextFragment::GetMainKey() const {
  return HWFragmentKeyType::kSDFText;
}

uint32_t WGSLSdfColorTextFragment::NextBindingIndex() const { return 6; }

void WGSLSdfColorTextFragment::WriteFSUniforms(std::stringstream& ss) const {
  WGSLTextFragment::WriteFSUniforms(ss);
  ss << R"(
@group(1) @binding(5) var<uniform> uColor: vec4<f32>;
)";
}

void WGSLSdfColorTextFragment::WriteFSMain(std::stringstream& ss) const {
  ss << R"(
  color = vec4<f32>(uColor.rgb * uColor.a, uColor.a);
)";
}

void WGSLSdfColorTextFragment::WriteFSCoverage(std::stringstream& ss) const {
  ss << R"(
  var distance: f32 =
      7.96875 * (get_texture_color(input.v_txt_index, input.v_uv).r - 0.5019608);
  var distance_gradient: vec2<f32> = vec2<f32>(dFdx(distance), dFdy(distance));
  let gradient_length_squared: f32 =
      dot(distance_gradient, distance_gradient);
  if gradient_length_squared < 0.0001 {
    distance_gradient = vec2<f32>(0.7071);
  } else {
    distance_gradient =
        distance_gradient * inversesqrt(gradient_length_squared);
  }
  let jacobian: mat2x2<f32> =
      mat2x2<f32>(dFdx(input.v_uv), dFdy(input.v_uv));
  let gradient: vec2<f32> = jacobian * distance_gradient;
  let antialias_width: f32 = 0.65 * length(gradient);
  coverage = smoothstep(-antialias_width, antialias_width, distance);
)";
}

void WGSLSdfColorTextFragment::PrepareCMD(Command* cmd,
                                          HWDrawContext* context) {
  WGSLTextFragment::PrepareCMD(cmd, context);
  if (cmd == nullptr || cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);
  if (group == nullptr) {
    return;
  }
  auto entry = group->GetEntry(5);
  if (entry == nullptr || entry->type_definition == nullptr ||
      entry->type_definition->name != "vec4<f32>") {
    return;
  }

  entry->type_definition->SetData(&color_, sizeof(Color4f));
  UploadBindGroup(group->group, entry, cmd, context);

  if (filter_ != nullptr) {
    filter_->SetupBindGroup(cmd, context);
  }
}

bool WGSLSdfColorTextFragment::CanMerge(const HWWGSLFragment* other) const {
  if (GetMainKey() != other->GetMainKey() ||
      !WGSLTextFragment::CanMerge(other)) {
    return false;
  }
  auto o = static_cast<const WGSLSdfColorTextFragment*>(other);
  return color_ == o->color_;
}

}  // namespace skity
