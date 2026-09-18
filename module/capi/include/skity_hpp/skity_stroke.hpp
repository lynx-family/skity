// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_STROKE_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_STROKE_HPP

// Stroke wrapper of the header-only RAII layer: converts a path's stroke
// (per a paint's stroke parameters) into its fill outline. The C layer
// exposes free functions; the legacy helper was likewise free-standing. See
// skity.hpp for the layer's design.

#include <skity_c/skity_stroke.h>

#include <skity_hpp/skity_paint.hpp>
#include <skity_hpp/skity_path.hpp>

namespace skity {
namespace raii {

/**
 * Append the stroke outline of @p src (per @p paint's width / cap / join /
 * miter; @p paint should be set to a stroke style) to @p dst.
 */
inline void StrokePath(const Paint& paint, const Path& src, Path* dst) {
  skity_stroke_stroke_path(paint.get(), src.get(), dst ? dst->get() : nullptr);
}

/**
 * Append the quadratic / cubic stroke outline of @p src to @p dst;
 * @p keep_cubic preserves cubic segments instead of flattening them.
 */
inline void QuadPath(const Paint& paint, const Path& src, Path* dst,
                     bool keep_cubic = true) {
  skity_stroke_quad_path(paint.get(), src.get(), dst ? dst->get() : nullptr,
                         keep_cubic ? 1u : 0u);
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_STROKE_HPP
