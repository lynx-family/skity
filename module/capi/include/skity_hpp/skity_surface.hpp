// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SURFACE_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SURFACE_HPP

// GPU surface wrapper of the header-only RAII layer: owns a render target
// and hands out borrowed Canvas views per frame. See skity.hpp for the
// layer's design.

#include <skity_c/skity_surface.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_canvas.hpp>
#include <skity_hpp/skity_context.hpp>

namespace skity {
namespace raii {

/** GPU surface wrapper: owns a render target and its canvas. */
class Surface : public detail::OwnHandle<skity_surface, skity_surface_destroy> {
 public:
  /**
   * Create a surface on @p context; the backend is selected by the
   * p_next extension of @p info (see skity_surface_create_info_gl etc.).
   * On failure @p out is left empty and the result code is returned.
   */
  static skity_result Create(const Context& context,
                             const skity_surface_create_info& info,
                             Surface* out) {
    skity_surface handle = nullptr;
    skity_result result = skity_surface_create(context.get(), &info, &handle);
    if (result == SKITY_SUCCESS && out != nullptr) {
      out->reset(handle);
    }
    return result;
  }

  /**
   * Lock the canvas for the current frame. The returned view (and the
   * underlying canvas) stays valid until the next LockCanvas call or
   * until the surface is destroyed. Call Canvas::Flush, then Flush().
   */
  Canvas LockCanvas(bool clear = true) {
    return Canvas(skity_surface_lock_canvas(get(), clear ? 1u : 0u));
  }

  /** Present the rendering result; the canvas must be flushed first. */
  void Flush() { skity_surface_flush(get()); }

  uint32_t GetWidth() const { return skity_surface_get_width(get()); }
  uint32_t GetHeight() const { return skity_surface_get_height(get()); }
  float GetContentScale() const {
    return skity_surface_get_content_scale(get());
  }
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SURFACE_HPP
