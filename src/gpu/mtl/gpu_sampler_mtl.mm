// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "src/gpu/mtl/gpu_sampler_mtl.h"

#include <memory>

#include "src/gpu/mtl/formats_mtl.h"
#include "src/gpu/mtl/gpu_device_mtl.h"

namespace skity {

std::shared_ptr<GPUSamplerMTL> GPUSamplerMTL::Create(GPUDeviceMTL& device,
                                                     const GPUSamplerDescriptor& desc) {
  id<MTLSamplerState> mtl_sampler =
      [device.GetMTLDevice() newSamplerStateWithDescriptor:ToMTLSamplerDescriptor(desc)];
  return std::make_shared<GPUSamplerMTL>(mtl_sampler, desc);
}

GPUSamplerMTL::GPUSamplerMTL(id<MTLSamplerState> sampler, const GPUSamplerDescriptor& descriptor)
    : GPUSampler(descriptor), mtl_sampler_(sampler) {}

GPUSamplerMTL::~GPUSamplerMTL() = default;

id<MTLSamplerState> GPUSamplerMTL::GetMTLSampler() const { return mtl_sampler_; }

}  // namespace skity
