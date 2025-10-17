// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_EFFECT_MASK_FILTER_HPP
#define INCLUDE_SKITY_EFFECT_MASK_FILTER_HPP

#include <memory>
#include <skity/geometry/rect.hpp>
#include <skity/io/flattenable.hpp>
#include <skity/macros.hpp>

namespace skity {

enum BlurStyle : int {
  kNormal = 1,  // fuzzy inside and outside
  kSolid,       // solid inside, fuzzy outside
  kOuter,       // nothing inside, fuzzy outside
  kInner,       // fuzzy inside, nothing outside
};

class SKITY_API MaskFilter : public Flattenable {
 public:
  MaskFilter() = default;
  ~MaskFilter() override = default;
  MaskFilter& operator=(const MaskFilter&) = delete;

  BlurStyle GetBlurStyle() const { return style_; }

  float GetBlurRadius() const { return radius_; }

  std::string_view ProcName() const override;

  void FlattenToBuffer(WriteBuffer& buffer) const override;

  /**
   * Create a blur mask filter
   *
   * @param style BlurStyle to use
   * @param radius Radius of the Gaussian blur to apply. Must be > 0.
   *
   * @return blur mask instance
   */
  static std::shared_ptr<MaskFilter> MakeBlur(BlurStyle style, float radius);

 private:
  BlurStyle style_ = {};
  float radius_ = {};
};

}  // namespace skity

#endif  // INCLUDE_SKITY_EFFECT_MASK_FILTER_HPP
