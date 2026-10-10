// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_NATIVE_WINDOW_VK_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_NATIVE_WINDOW_VK_HPP

// VK-only part of the header-only RAII layer: native window presenter
// lifecycle (swapchain management, per-frame surface acquire / present).
// Intentionally NOT included by skity.hpp — including it pulls in
// <vulkan/vulkan.h>. See skity_vk.hpp for the VK aggregator.

#include <skity_c/skity_native_window_vk.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_context.hpp>
#include <skity_hpp/skity_surface.hpp>

namespace skity {
namespace raii {

/**
 * Vulkan native window presenter: owns a swapchain-bound window and hands
 * out one-shot Surfaces per frame.
 *
 * Frame loop: AcquireNextSurface -> Surface::LockCanvas / draw / Flush ->
 * Present. The acquired surface must be presented before acquiring the
 * next one; Present consumes the surface wrapper on SKITY_SUCCESS and on
 * SKITY_ERROR_NEED_RECREATE (the handle is then invalid — the wrapper is
 * emptied), and leaves it valid on other errors so it can be retried or
 * destroyed.
 */
class NativeWindowVk
    : public detail::OwnHandle<skity_native_window_vk,
                               skity_native_window_destroy_vk> {
 public:
  /** Create a window + initial swapchain; empty on failure. */
  static NativeWindowVk Create(const Context& context,
                               const skity_native_window_create_info_vk& info) {
    return Adopt(skity_native_window_create_vk(context.get(), &info));
  }

  /** Wrap a handle created through the raw C API. */
  static NativeWindowVk Adopt(skity_native_window_vk handle) {
    return NativeWindowVk(handle);
  }

  uint32_t GetWidth() const { return skity_native_window_get_width_vk(get()); }
  uint32_t GetHeight() const {
    return skity_native_window_get_height_vk(get());
  }

  /** Recreate the presenter / swapchain for a new physical size. */
  skity_result Resize(uint32_t width, uint32_t height) {
    return skity_native_window_resize_vk(get(), width, height);
  }

  /**
   * Acquire the next one-shot render surface into @p out. Only one surface
   * may be outstanding; present it before acquiring again.
   */
  skity_result AcquireNextSurface(uint32_t sample_count, float content_scale,
                                  Surface* out) {
    skity_surface handle = nullptr;
    skity_result result = skity_native_window_acquire_next_surface_vk(
        get(), sample_count, content_scale, &handle);
    if (result == SKITY_SUCCESS && out != nullptr) {
      out->reset(handle);
    }
    return result;
  }

  /**
   * Present an acquired surface. On SKITY_SUCCESS and
   * SKITY_ERROR_NEED_RECREATE the C layer consumes the handle and the
   * wrapper is emptied; on other errors @p surface keeps its handle.
   */
  skity_result Present(Surface* surface) {
    if (surface == nullptr || !*surface) {
      return SKITY_ERROR_INVALID_HANDLE;
    }
    skity_surface handle = surface->get();
    skity_result result = skity_native_window_present_vk(get(), &handle);
    if (result == SKITY_SUCCESS || result == SKITY_ERROR_NEED_RECREATE) {
      (void)surface->release();
    }
    return result;
  }

  /** Empty window (e.g. awaiting a Create result); all queries are no-ops. */
  NativeWindowVk() = default;

 private:
  explicit NativeWindowVk(skity_native_window_vk handle) : OwnHandle(handle) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_NATIVE_WINDOW_VK_HPP
