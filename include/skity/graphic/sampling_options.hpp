// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_SAMPLING_OPTIONS_HPP
#define INCLUDE_SKITY_GRAPHIC_SAMPLING_OPTIONS_HPP

#include <stdint.h>

#include <unordered_map>

namespace skity {

enum class FilterMode {
  kNearest,  // single sample point (nearest neighbor)
  kLinear,   // interporate between 2x2 sample points (bilinear interpolation)
};

enum class MipmapMode {
  kNone,     // ignore mipmap levels, sample from the "base"
  kNearest,  // sample from the nearest level
  kLinear,   // interpolate between the two nearest levels
};

struct SamplingOptions {
  FilterMode filter = FilterMode::kNearest;
  MipmapMode mipmap = MipmapMode::kNone;

  SamplingOptions() = default;

  constexpr SamplingOptions(FilterMode fm, MipmapMode mm)
      : filter(fm), mipmap(mm) {}

  struct Hash {
    std::size_t operator()(SamplingOptions const& key) const {
      size_t res = 17;
      res = res * 31 + std::hash<uint32_t>()(static_cast<uint32_t>(key.filter));
      res = res * 31 + std::hash<uint32_t>()(static_cast<uint32_t>(key.mipmap));
      return res;
    }
  };

  struct Equal {
    constexpr bool operator()(const SamplingOptions& lhs,
                              const SamplingOptions& rhs) const {
      return lhs.filter == rhs.filter && lhs.mipmap == rhs.mipmap;
    }
  };

  template <class Value>
  using Map = std::unordered_map<SamplingOptions, Value, Hash, Equal>;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_SAMPLING_OPTIONS_HPP
