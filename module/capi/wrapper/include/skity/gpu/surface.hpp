// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_SURFACE_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_SURFACE_HPP

#include <skity/c/skity_surface.h>

#include <cstdint>
#include <memory>
#include <skity/base.hpp>
#include <skity/gpu/context.hpp>
#include <skity/gpu/semaphore.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/render/canvas.hpp>
#include <skity/types.hpp>

namespace skity {
namespace detail {
struct SurfaceCreate;
}  // namespace detail

/**
 * @class Surface
 *
 * GPU surface wrapper: owns a render target and its canvas.
 *
 */
class Surface : public detail::OwnHandle<skity_surface, skity_surface_destroy> {
 public:
  /**
   * Adopt a surface handle created through the raw C API — e.g. one
   * acquired from skity_native_window_acquire_next_surface_vk. The wrapper
   * takes ownership; destroying it releases (invalidates) the surface.
   */
  static Surface Adopt(skity_surface handle) { return Surface(handle); }

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

  /**
   * Read pixels back to CPU memory (rect selects a sub-region; an empty
   * rect reads the whole surface). Returns an empty wrapper on failure.
   */
  std::shared_ptr<Pixmap> ReadPixels(
      const Rect& rect = Rect::MakeEmpty()) const {
    bool whole = rect.IsEmpty();
    auto handle = skity_surface_read_pixels(get(), whole ? nullptr : &rect);
    return handle != nullptr ? std::shared_ptr<Pixmap>(new Pixmap(handle))
                             : nullptr;
  }

  uint32_t GetWidth() const { return skity_surface_get_width(get()); }
  uint32_t GetHeight() const { return skity_surface_get_height(get()); }
  float GetContentScale() const {
    return skity_surface_get_content_scale(get());
  }

  /**
   * Wait on semaphore during the next flush (external GPU work feeding
   * this surface). Call between LockCanvas and Flush; consumed by the flush.
   */
  void AddExternalWaitSemaphore(const Semaphore& semaphore) {
    skity_surface_add_external_wait_semaphore(get(), semaphore.get());
  }

  /** Empty surface; filled by Create / AcquireNextSurface out-params. */
  Surface() = default;

 private:
  explicit Surface(skity_surface h) : OwnHandle(h) {}
  friend struct detail::SurfaceCreate;
  friend class GPUPresenter;
};

namespace detail {
/** Internal: backend creation helpers (skity_vk) construct through here. */
struct SurfaceCreate {
  static std::unique_ptr<Surface> New(skity_surface h) {
    return std::unique_ptr<Surface>(new Surface(h));
  }
};
}  // namespace detail

inline std::unique_ptr<Surface> Context::CreateSurface(
    const skity_surface_create_info& info) const {
  skity_surface handle = nullptr;
  if (skity_surface_create(get(), &info, &handle) != SKITY_SUCCESS) {
    return nullptr;
  }
  return detail::SurfaceCreate::New(handle);
}

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_SURFACE_HPP
