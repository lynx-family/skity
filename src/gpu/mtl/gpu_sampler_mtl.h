// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_SAMPLER_MTL_H
#define SRC_GPU_MTL_GPU_SAMPLER_MTL_H

#import <Metal/Metal.h>

#include <memory>

#include "src/gpu/backend_cast.hpp"
#include "src/gpu/gpu_sampler.hpp"

namespace skity {

class GPUDeviceMTL;

class GPUSamplerMTL : public GPUSampler {
 public:
  static std::shared_ptr<GPUSamplerMTL> Create(
      GPUDeviceMTL& device, const GPUSamplerDescriptor& descriptor);

  GPUSamplerMTL(id<MTLSamplerState> sampler,
                const GPUSamplerDescriptor& descriptor);

  ~GPUSamplerMTL() override;

  id<MTLSamplerState> GetMTLSampler() const;

  SKT_BACKEND_CAST(GPUSamplerMTL, GPUSampler)

 private:
  id<MTLSamplerState> mtl_sampler_;
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_SAMPLER_MTL_H
