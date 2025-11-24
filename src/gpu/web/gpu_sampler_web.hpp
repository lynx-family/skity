// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_GPU_SAMPLER_WEB_HPP
#define SRC_GPU_WEB_GPU_SAMPLER_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_sampler.hpp"

namespace skity {

class GPUSamplerWEB : public GPUSampler {
 public:
  GPUSamplerWEB(const GPUSamplerDescriptor& desc, WGPUSampler sampler);

  ~GPUSamplerWEB() override;

  WGPUSampler GetSampler() const { return sampler_; }

  static std::shared_ptr<GPUSamplerWEB> Create(
      WGPUDevice device, const GPUSamplerDescriptor& desc);

 private:
  WGPUSampler sampler_ = nullptr;
};

}  // namespace skity

#endif  // SRC_GPU_WEB_GPU_SAMPLER_WEB_HPP
