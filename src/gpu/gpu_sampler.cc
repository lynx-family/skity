// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gpu_sampler.hpp"

namespace skity {

GPUFilterMode ToGPUFilterMode(FilterMode filter_mode) {
  switch (filter_mode) {
    case FilterMode::kLinear:
      return GPUFilterMode::kLinear;
    case FilterMode::kNearest:
    default:
      return GPUFilterMode::kNearest;
  }
}

GPUMipmapMode ToGPUMipmapMode(MipmapMode mipmap_mode) {
  switch (mipmap_mode) {
    case MipmapMode::kLinear:
      return GPUMipmapMode::kLinear;
    case MipmapMode::kNearest:
      return GPUMipmapMode::kNearest;
    case MipmapMode::kNone:
    default:
      return GPUMipmapMode::kNone;
  }
}

GPUAddressMode ToGPUAddressMode(TileMode tile_mode) {
  switch (tile_mode) {
    case TileMode::kRepeat:
      return GPUAddressMode::kRepeat;
    case TileMode::kMirror:
      return GPUAddressMode::kMirrorRepeat;
    case TileMode::kClamp:
    case TileMode::kDecal:
    default:
      return GPUAddressMode::kClampToEdge;
  }
}

GPUSamplerDescriptor GPUSamplerDescriptor::CreateGPUSamplerDescriptor(
    const SamplingOptions& options) {
  GPUSamplerDescriptor descriptor;

  switch (options.filter) {
    case FilterMode::kLinear:
      descriptor.mag_filter = descriptor.min_filter = GPUFilterMode::kLinear;
      break;
    case FilterMode::kNearest:
    default:
      descriptor.mag_filter = descriptor.min_filter = GPUFilterMode::kNearest;
      break;
  }

  switch (options.mipmap) {
    case MipmapMode::kNearest:
      descriptor.mipmap_filter = GPUMipmapMode::kNearest;
      break;
    case MipmapMode::kLinear:
      descriptor.mipmap_filter = GPUMipmapMode::kLinear;
      break;
    case MipmapMode::kNone:
    default:
      descriptor.mipmap_filter = GPUMipmapMode::kNone;
      break;
  }

  return descriptor;
}

GPUSampler::GPUSampler(const GPUSamplerDescriptor& desc) : desc_(desc) {}

GPUSampler::~GPUSampler() = default;

const GPUSamplerDescriptor& GPUSampler::GetDescriptor() const { return desc_; }

}  // namespace skity
