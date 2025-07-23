// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_SAMPLER_HPP
#define SRC_GPU_GPU_SAMPLER_HPP

#include <memory>
#include <skity/graphic/sampling_options.hpp>
#include <skity/graphic/tile_mode.hpp>

namespace skity {

enum class GPUAddressMode {
  kClampToEdge,
  kRepeat,
  kMirrorRepeat,
};

enum class GPUFilterMode {
  kNearest,
  kLinear,
};

enum class GPUMipmapMode {
  kNone,
  kNearest,
  kLinear,
};

GPUFilterMode ToGPUFilterMode(FilterMode filter_mode);
GPUMipmapMode ToGPUMipmapMode(MipmapMode mipmap_mode);
GPUAddressMode ToGPUAddressMode(TileMode tile_mode);

struct SamplingOptions;
struct GPUSamplerDescriptor {
  GPUAddressMode address_mode_u = GPUAddressMode::kClampToEdge;
  GPUAddressMode address_mode_v = GPUAddressMode::kClampToEdge;
  GPUAddressMode address_mode_w = GPUAddressMode::kClampToEdge;
  GPUFilterMode mag_filter = GPUFilterMode::kNearest;
  GPUFilterMode min_filter = GPUFilterMode::kNearest;
  GPUMipmapMode mipmap_filter = GPUMipmapMode::kNone;

  static GPUSamplerDescriptor CreateGPUSamplerDescriptor(
      const SamplingOptions& options);
};

class GPUSampler {
 public:
  explicit GPUSampler(const GPUSamplerDescriptor& desc);
  virtual ~GPUSampler() = 0;
  const GPUSamplerDescriptor& GetDescriptor() const;

 protected:
  GPUSamplerDescriptor desc_;
};

struct GPUSamplerEqual {
  constexpr bool operator()(const GPUSamplerDescriptor& lhs,
                            const GPUSamplerDescriptor& rhs) const {
    return lhs.address_mode_u == rhs.address_mode_u &&  //
           lhs.address_mode_v == rhs.address_mode_v &&  //
           lhs.address_mode_w == rhs.address_mode_w &&  //
           lhs.mag_filter == rhs.mag_filter &&          //
           lhs.min_filter == rhs.min_filter &&          //
           lhs.mipmap_filter == rhs.mipmap_filter;      //
  }
};

struct GPUSamplerHash {
  std::size_t operator()(const GPUSamplerDescriptor& key) const {
    size_t res = 17;
    res = res * 31 +
          std::hash<uint32_t>()(static_cast<uint32_t>(key.address_mode_u));
    res = res * 31 +
          std::hash<uint32_t>()(static_cast<uint32_t>(key.address_mode_v));
    res = res * 31 +
          std::hash<uint32_t>()(static_cast<uint32_t>(key.address_mode_w));
    res =
        res * 31 + std::hash<uint32_t>()(static_cast<uint32_t>(key.mag_filter));
    res =
        res * 31 + std::hash<uint32_t>()(static_cast<uint32_t>(key.min_filter));
    res = res * 31 +
          std::hash<uint32_t>()(static_cast<uint32_t>(key.mipmap_filter));
    return res;
  }
};

using GPUSamplerMap =
    std::unordered_map<GPUSamplerDescriptor, std::shared_ptr<GPUSampler>,
                       GPUSamplerHash, GPUSamplerEqual>;

}  // namespace skity

#endif  // SRC_GPU_GPU_SAMPLER_HPP
