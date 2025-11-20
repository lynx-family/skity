// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_blur_filter.hpp"

#include <wgsl_cross.h>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLBlurFilter::WGSLBlurFilter(std::shared_ptr<GPUTexture> texture, Vec2 dir,
                               float radius, Vec2 uv_scale, Vec2 uv_offset)
    : texture_(std::move(texture)),
      dir_(dir),
      radius_(radius),
      uv_scale_(uv_scale),
      uv_offset_(uv_offset) {}

uint32_t WGSLBlurFilter::NextBindingIndex() const { return 3; }

std::string WGSLBlurFilter::GetShaderName() const { return "BlurFragmentWGSL"; }

std::string WGSLBlurFilter::GenSourceWGSL() const {
  return R"(
    struct BlurFragSlot {
      dir        : vec2<f32>,
      uv_scale   : vec2<f32>,
      uv_offset  : vec2<f32>,
      radius     : f32,
    };


    @group(1) @binding(0) var<uniform> blur_slot        : BlurFragSlot;
    @group(1) @binding(1) var          uSampler         : sampler;
    @group(1) @binding(2) var          uTexture         : texture_2d<f32>;

    fn convert_radius_to_sigma(radius: f32) -> f32 {
      if radius > 0.0 {
        return radius * 0.57735 + 0.5;
      } else {
        return 0.0;
      }
    }

    fn calculate_blur_norm(radius: f32) -> f32 {
      var sigma : f32 = convert_radius_to_sigma(radius);

      return 1.0 / (sqrt(2.0 * 3.1415926) * sigma);
    }

    fn calculate_blur_coffe(radius: f32, norm: f32, step: f32) -> f32 {
      var sigma: f32 = convert_radius_to_sigma(radius);

      return norm * exp(-0.5 * step * step / (sigma * sigma));
    }

    fn decl_texture(uv: vec2<f32>) -> vec4<f32> {
      if uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 {
        return vec4<f32>(0.0, 0.0, 0.0, 0.0);
      }

      return textureSample(uTexture, uSampler, uv);
    }

    fn calculate_blur(uv: vec2<f32>, dir: vec2<f32>, radius: f32) -> vec4<f32> {
      var norm     : f32       = calculate_blur_norm(radius);
      var total    : f32       = norm;
      var acc      : vec4<f32> = decl_texture(uv) * norm;

      var kernel_size : i32 = i32(radius);

      for (var i: i32 = 1; i <= kernel_size; i = i + 1) {
        var coffe  : f32       = calculate_blur_coffe(radius, norm, f32(i));
        var offset : vec2<f32> = dir * f32(i);

        acc += decl_texture(uv - offset) * coffe;
        acc += decl_texture(uv + offset) * coffe;

        total += 2.0 * coffe;
      }

      acc = acc / total;

      return acc;
    }

    @fragment
    fn fs_main(@location(0) v_uv: vec2<f32>) -> @location(0) vec4<f32> {
      var blur_radius : f32       = blur_slot.radius;
      var dir         : vec2<f32> = blur_slot.dir;
      var uv          : vec2<f32> = v_uv * blur_slot.uv_scale + blur_slot.uv_offset;

      if blur_radius > 0.0 {
        return calculate_blur(uv, dir, blur_radius);
      } else {
        return textureSample(uTexture, uSampler, uv);
      }
    }
  )";
}

void WGSLBlurFilter::PrepareCMD(Command* cmd, HWDrawContext* context) {
  SKITY_TRACE_EVENT(WGSLBlurFilter_PrepareCMD);

  if (cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);
  if (group == nullptr) {
    return;
  }

  auto slot_entry = group->GetEntry(0);

  if (slot_entry == nullptr ||
      slot_entry->type != wgx::BindingType::kUniformBuffer ||
      slot_entry->type_definition->name != "BlurFragSlot") {
    return;
  }

  {
    auto slot_struct =
        static_cast<wgx::StructDefinition*>(slot_entry->type_definition.get());
    slot_struct->GetMember("radius")->type->SetData(radius_);
    slot_struct->GetMember("uv_scale")->type->SetData(uv_scale_);
    slot_struct->GetMember("uv_offset")->type->SetData(uv_offset_);

    auto width = texture_->GetDescriptor().width;
    auto height = texture_->GetDescriptor().height;

    Vec2 dir{
        dir_.x / static_cast<float>(width),
        dir_.y / static_cast<float>(height),
    };

    slot_struct->GetMember("dir")->type->SetData(dir);

    UploadBindGroup(group->group, slot_entry, cmd, context);
  }

  auto sampler_entry = group->GetEntry(1);

  if (sampler_entry == nullptr ||
      sampler_entry->type != wgx::BindingType::kSampler) {
    return;
  }

  {
    GPUSamplerDescriptor desc;
    desc.mag_filter = GPUFilterMode::kLinear;
    desc.min_filter = GPUFilterMode::kLinear;

    auto sampler = context->gpuContext->GetGPUDevice()->CreateSampler(desc);

    UploadBindGroup(group->group, sampler_entry, cmd, sampler);
  }

  auto texture_entry = group->GetEntry(2);

  if (texture_entry == nullptr ||
      texture_entry->type != wgx::BindingType::kTexture) {
    return;
  }

  UploadBindGroup(group->group, texture_entry, cmd, texture_);
}

}  // namespace skity
