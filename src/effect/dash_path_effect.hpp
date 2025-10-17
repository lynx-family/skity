// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_EFFECT_DASH_PATH_EFFECT_HPP
#define SRC_EFFECT_DASH_PATH_EFFECT_HPP

#include <memory>
#include <skity/effect/path_effect.hpp>

namespace skity {

class DashPathEffect : public PathEffect {
 public:
  DashPathEffect(const float intervals[], int32_t count, float phase);

  ~DashPathEffect() override = default;

  std::string_view ProcName() const override;

  void FlattenToBuffer(WriteBuffer &buffer) const override;

 protected:
  bool OnFilterPath(Path *, const Path &, bool, Paint const &) const override;

  DashType OnAsADash(DashInfo *) const override;

 private:
  /**
   * Update internal phase_, initial_dash_length_, initial_dash_index_,
   * internal_length_
   */
  void CalcDashParameters(float phase);

 private:
  std::unique_ptr<float[]> intervals_;
  int32_t count_ = 0;
  float phase_;

  float initial_dash_length_;
  int32_t initial_dash_index_;
  float interval_length_;
};

}  // namespace skity

#endif  // SRC_EFFECT_DASH_PATH_EFFECT_HPP
