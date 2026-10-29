// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_PATH_EFFECT_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_PATH_EFFECT_HPP

#include <skity/c/skity_path_effect.h>

#include <cstdint>
#include <memory>
#include <skity/base.hpp>

namespace skity {

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
  static std::shared_ptr<PathEffect> MakeDiscretePathEffect(
      float seg_length, float dev, uint32_t seed_assist = 0) {
    auto handle =
        skity_path_effect_create_discrete(seg_length, dev, seed_assist);
    return handle != nullptr
               ? std::shared_ptr<PathEffect>(new PathEffect(handle))
               : nullptr;
  }

  /** Stroke as dashes: intervals of on/off lengths (even count). */
  static std::shared_ptr<PathEffect> MakeDashPathEffect(const float* intervals,
                                                        int32_t count,
                                                        float phase = 0.f) {
    auto handle = skity_path_effect_create_dash(intervals, count, phase);
    return handle != nullptr
               ? std::shared_ptr<PathEffect>(new PathEffect(handle))
               : nullptr;
  }

  /**
   * Adopt an owning C handle (e.g. from skity_paint_get_path_effect);
   * destroying the wrapper releases the shared reference.
   */
  static PathEffect Adopt(skity_path_effect h) { return PathEffect(h); }

  friend class Paint;

 private:
  explicit PathEffect(skity_path_effect h) : OwnHandle(h) {}
};

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_PATH_EFFECT_HPP
