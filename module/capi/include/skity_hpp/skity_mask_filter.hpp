// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_MASK_FILTER_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_MASK_FILTER_HPP

// MaskFilter wrapper of the header-only RAII layer: modifies the alpha mask
// of a drawing primitive before rasterization (Gaussian blur today). See
// skity.hpp for the layer's design.

#include <skity_c/skity_mask_filter.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>

namespace skity {
namespace raii {

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

/** Mask filter wrapper. Attach with Paint::SetMaskFilter. */
class MaskFilter
    : public detail::OwnHandle<skity_mask_filter, skity_mask_filter_destroy> {
 public:
  /** Gaussian blur of the alpha mask; @p radius in pixels (> 0). */
  static MaskFilter MakeBlur(BlurStyle style, float radius) {
    return MaskFilter(skity_mask_filter_create_blur(to_c(style), radius));
  }

  /**
   * Adopt an owning C handle (e.g. from skity_paint_get_mask_filter);
   * destroying the wrapper releases the shared reference.
   */
  static MaskFilter Adopt(skity_mask_filter h) { return MaskFilter(h); }

 private:
  explicit MaskFilter(skity_mask_filter h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_MASK_FILTER_HPP
