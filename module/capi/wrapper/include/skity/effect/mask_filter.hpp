// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_MASK_FILTER_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_MASK_FILTER_HPP

#include <skity/c/skity_mask_filter.h>

#include <cstdint>
#include <memory>
#include <skity/base.hpp>

namespace skity {

/** Blur style, mirroring the legacy skity::BlurStyle enumerators. */
enum class BlurStyle : uint32_t {
  kNormal = SKITY_BLUR_STYLE_NORMAL,
  kSolid = SKITY_BLUR_STYLE_SOLID,
  kOuter = SKITY_BLUR_STYLE_OUTER,
  kInner = SKITY_BLUR_STYLE_INNER,
};

inline constexpr skity_blur_style to_c(BlurStyle style) {
  return static_cast<skity_blur_style>(style);
}

/**
 * @class MaskFilter
 *
 * Mask filter wrapper. Attach with Paint::SetMaskFilter.
 *
 */
class MaskFilter
    : public detail::OwnHandle<skity_mask_filter, skity_mask_filter_destroy> {
 public:
  /** Gaussian blur of the alpha mask; radius in pixels (> 0). */
  static std::shared_ptr<MaskFilter> MakeBlur(BlurStyle style, float radius) {
    auto handle = skity_mask_filter_create_blur(to_c(style), radius);
    return handle != nullptr
               ? std::shared_ptr<MaskFilter>(new MaskFilter(handle))
               : nullptr;
  }

  /**
   * Adopt an owning C handle (e.g. from skity_paint_get_mask_filter);
   * destroying the wrapper releases the shared reference.
   */
  static MaskFilter Adopt(skity_mask_filter h) { return MaskFilter(h); }

  friend class Paint;

 private:
  explicit MaskFilter(skity_mask_filter h) : OwnHandle(h) {}
};

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_EFFECT_MASK_FILTER_HPP
