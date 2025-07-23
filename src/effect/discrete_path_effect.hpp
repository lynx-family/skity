// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_EFFECT_DISCRETE_PATH_EFFECT_HPP
#define SRC_EFFECT_DISCRETE_PATH_EFFECT_HPP

#include <skity/effect/path_effect.hpp>

namespace skity {

class DiscretePathEffect : public PathEffect {
 public:
  DiscretePathEffect(float seg_length, float deviation, uint32_t seed_assist);
  ~DiscretePathEffect() override = default;

 protected:
  bool OnFilterPath(Path* dst, Path const& src, bool stroke,
                    Paint const&) const override;

 private:
  float seg_length_;
  float perterb_;
  uint32_t seed_assist_;
};

}  // namespace skity

#endif  // SRC_EFFECT_DISCRETE_PATH_EFFECT_HPP
