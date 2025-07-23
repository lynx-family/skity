
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_image_filter.hpp"

#include "src/gpu/gpu_context_impl.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/tracing.hpp"

namespace skity {

WGSLImageFilter::WGSLImageFilter(std::shared_ptr<GPUTexture> texture)
    : texture_(std::move(texture)) {}

std::string WGSLImageFilter::GetShaderName() const {
  std::string name = "ImageFilterFragmentWGSL";

  if (filter_) {
    name += filter_->GetShaderName();
  }

  return name;
}

std::string WGSLImageFilter::GenSourceWGSL() const {
  std::string wgsl_code = R"(
    @group(1) @binding(0) var uSampler    : sampler;
    @group(1) @binding(1) var uTexture    : texture_2d<f32>;
  )";

  if (filter_) {
    wgsl_code += filter_->GenSourceWGSL();
  }

  wgsl_code += R"(
    @fragment
    fn fs_main(@location(0) v_uv: vec2<f32>) -> @location(0) vec4<f32> {
      var color: vec4<f32> = textureSample(uTexture, uSampler, v_uv);
  )";

  if (filter_) {
    wgsl_code += R"(
      color = filter_color(color);
    )";
  }

  wgsl_code += R"(
      return color;
    }
  )";

  return wgsl_code;
}

const char* WGSLImageFilter::GetEntryPoint() const { return "fs_main"; }

uint32_t WGSLImageFilter::NextBindingIndex() const { return 2; }

void WGSLImageFilter::PrepareCMD(Command* cmd, HWDrawContext* context) {
  SKITY_TRACE_EVENT(WGSLImageFilter_PrepareCMD);

  if (cmd->pipeline == nullptr) {
    return;
  }

  auto group = cmd->pipeline->GetBindingGroup(1);

  auto sampler_entry = group->GetEntry(0);

  if (sampler_entry == nullptr ||
      sampler_entry->type != wgx::BindingType::kSampler) {
    return;
  }

  // sampler
  {
    GPUSamplerDescriptor desc{};
    desc.mag_filter = GPUFilterMode::kLinear;
    desc.min_filter = GPUFilterMode::kLinear;

    auto sampler = context->gpuContext->GetGPUDevice()->CreateSampler(desc);

    UploadBindGroup(sampler_entry, cmd, sampler);
  }

  auto texture_entry = group->GetEntry(1);

  if (texture_entry == nullptr ||
      texture_entry->type != wgx::BindingType::kTexture) {
    return;
  }

  UploadBindGroup(texture_entry, cmd, texture_);

  if (filter_) {
    filter_->SetupBindGroup(cmd, context);
  }
}

}  // namespace skity
