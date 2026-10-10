// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_EFFECT_PATH_EFFECT_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_EFFECT_PATH_EFFECT_HPP

#include <skity_c/skity_path_effect.h>

#include <cstdint>
#include <skity_hpp/base.hpp>

namespace skity {
namespace raii {

/**
 * @class PathEffect
 *
 * Path effect wrapper. Attach with Paint::SetPathEffect.
 *
 */
class PathEffect
    : public detail::OwnHandle<skity_path_effect, skity_path_effect_destroy> {
 public:
  /** Randomly displace path vertices: segments of seg_length shifted up
   *  to dev. seed_assist 0 is deterministic. */
  static PathEffect MakeDiscretePathEffect(float seg_length, float dev,
                                           uint32_t seed_assist = 0) {
    return PathEffect(
        skity_path_effect_create_discrete(seg_length, dev, seed_assist));
  }

  /** Stroke as dashes: intervals of on/off lengths (even count). */
  static PathEffect MakeDashPathEffect(const float* intervals, int32_t count,
                                       float phase = 0.f) {
    return PathEffect(skity_path_effect_create_dash(intervals, count, phase));
  }

  /**
   * Adopt an owning C handle (e.g. from skity_paint_get_path_effect);
   * destroying the wrapper releases the shared reference.
   */
  static PathEffect Adopt(skity_path_effect h) { return PathEffect(h); }

 private:
  explicit PathEffect(skity_path_effect h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_EFFECT_PATH_EFFECT_HPP
