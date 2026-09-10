// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_EFFECT_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_EFFECT_HPP

// Path effect wrapper of the header-only RAII layer. Factory names keep the
// legacy C++ shapes (MakeDiscretePathEffect / MakeDashPathEffect) so example
// and consumer code compiles against both APIs unchanged. See skity.hpp for
// the layer's design.

#include <skity_c/skity_path_effect.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>

namespace skity {
namespace raii {

/** Path effect wrapper. Attach with Paint::SetPathEffect. */
class PathEffect
    : public detail::OwnHandle<skity_path_effect, skity_path_effect_destroy> {
 public:
  /** Randomly displace path vertices: segments of @p seg_length shifted up
   *  to @p dev. @p seed_assist 0 is deterministic. */
  static PathEffect MakeDiscretePathEffect(float seg_length, float dev,
                                           uint32_t seed_assist = 0) {
    return PathEffect(
        skity_path_effect_create_discrete(seg_length, dev, seed_assist));
  }

  /** Stroke as dashes: @p intervals of on/off lengths (even count). */
  static PathEffect MakeDashPathEffect(const float* intervals, int32_t count,
                                       float phase = 0.f) {
    return PathEffect(skity_path_effect_create_dash(intervals, count, phase));
  }

 private:
  explicit PathEffect(skity_path_effect h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_EFFECT_HPP
