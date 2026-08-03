// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_SAMPLING_OPTIONS_HPP
#define INCLUDE_SKITY_GRAPHIC_SAMPLING_OPTIONS_HPP

#include <stdint.h>

#include <functional>
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

/**
 * Cubic resampler coefficients (Mitchell-Netravali B/C family), mirroring
 * Skia's SkCubicResampler. When B and C are both zero, cubic sampling is
 * disabled and SamplingOptions falls back to FilterMode.
 *
 * Common presets:
 *   Catmull-Rom : B = 0,    C = 0.5
 *   Mitchell    : B = 1/3,  C = 1/3
 */
struct CubicResampler {
  float B = 0.f;
  float C = 0.f;

  /**
   * Whether cubic resampling is enabled. Mirrors Skia's convention where
   * B == 0 && C == 0 means cubic sampling is turned off.
   */
  constexpr bool UseCubic() const { return B != 0.f || C != 0.f; }
};

struct SamplingOptions {
  FilterMode filter = FilterMode::kNearest;
  MipmapMode mipmap = MipmapMode::kNone;
  CubicResampler cubic{};

  SamplingOptions() = default;

  constexpr SamplingOptions(FilterMode fm, MipmapMode mm)
      : filter(fm), mipmap(mm) {}

  /**
   * Whether cubic resampling is enabled (non-zero B/C).
   */
  constexpr bool UseCubic() const { return cubic.UseCubic(); }

  struct Hash {
    std::size_t operator()(SamplingOptions const& key) const {
      size_t res = 17;
      res = res * 31 + std::hash<uint32_t>()(static_cast<uint32_t>(key.filter));
      res = res * 31 + std::hash<uint32_t>()(static_cast<uint32_t>(key.mipmap));
      res = res * 31 + std::hash<float>()(key.cubic.B);
      res = res * 31 + std::hash<float>()(key.cubic.C);
      return res;
    }
  };

  struct Equal {
    constexpr bool operator()(const SamplingOptions& lhs,
                              const SamplingOptions& rhs) const {
      return lhs.filter == rhs.filter && lhs.mipmap == rhs.mipmap &&
             lhs.cubic.B == rhs.cubic.B && lhs.cubic.C == rhs.cubic.C;
    }
  };

  template <class Value>
  using Map = std::unordered_map<SamplingOptions, Value, Hash, Equal>;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_SAMPLING_OPTIONS_HPP
