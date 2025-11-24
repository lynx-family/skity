// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_sampler_web.hpp"

#include "src/gpu/web/format_web.hpp"

namespace skity {

namespace {

WGPUSamplerDescriptor ConvertToWGPUSamplerDescriptor(
    const GPUSamplerDescriptor& desc) {
  WGPUSamplerDescriptor wgpu_desc = {};

  wgpu_desc.addressModeU = ToWGPUAddressMode(desc.address_mode_u);
  wgpu_desc.addressModeV = ToWGPUAddressMode(desc.address_mode_v);
  wgpu_desc.addressModeW = ToWGPUAddressMode(desc.address_mode_w);
  wgpu_desc.magFilter = ToWGPUFilterMode(desc.mag_filter);
  wgpu_desc.minFilter = ToWGPUFilterMode(desc.min_filter);
  wgpu_desc.mipmapFilter = ToWGPUMipmapFilterMode(desc.mipmap_filter);

  wgpu_desc.lodMinClamp = 0.0f;
  wgpu_desc.lodMaxClamp = 2.0f;
  wgpu_desc.compare = WGPUCompareFunction_Undefined;
  wgpu_desc.maxAnisotropy = 1.0f;

  return wgpu_desc;
}

}  // namespace

GPUSamplerWEB::GPUSamplerWEB(const GPUSamplerDescriptor& desc,
                             WGPUSampler sampler)
    : GPUSampler(desc), sampler_(sampler) {}

GPUSamplerWEB::~GPUSamplerWEB() {
  if (sampler_) {
    wgpuSamplerRelease(sampler_);
  }
}

std::shared_ptr<GPUSamplerWEB> GPUSamplerWEB::Create(
    WGPUDevice device, const GPUSamplerDescriptor& desc) {
  auto wgpu_desc = ConvertToWGPUSamplerDescriptor(desc);

  WGPUSampler sampler = wgpuDeviceCreateSampler(device, &wgpu_desc);

  if (!sampler) {
    return {};
  }

  return std::make_shared<GPUSamplerWEB>(desc, sampler);
}

}  // namespace skity
