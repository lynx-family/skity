// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_PRESENTER_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_PRESENTER_HPP

#include <skity/c/skity_presenter.h>

#include <cstdint>
#include <memory>
#include <skity/base.hpp>
#include <skity/gpu/surface.hpp>

namespace skity {

/**
 * Result status for acquire / present, mirroring the legacy
 * skity::GPUPresenterStatus; mapped from the C result codes.
 */
enum class GPUPresenterStatus : uint32_t {
  kSuccess,
  kNeedRecreate,
  kError,
};

inline GPUPresenterStatus presenter_status_of(skity_result result) {
  if (result == SKITY_SUCCESS) {
    return GPUPresenterStatus::kSuccess;
  }
  if (result == SKITY_ERROR_NEED_RECREATE) {
    return GPUPresenterStatus::kNeedRecreate;
  }
  return GPUPresenterStatus::kError;
}

/**
 * Acquire descriptor, mirroring the legacy GPUSurfaceAcquireDescriptor.
 * (The legacy render_options member has no C counterpart and is omitted.)
 */
struct GPUSurfaceAcquireDescriptor {
  uint32_t sample_count = 1;
  float content_scale = 1.f;
};

/**
 * Acquire result, mirroring the legacy GPUSurfaceAcquireResult: on
 * kSuccess, surface holds the one-shot render surface.
 */
struct GPUSurfaceAcquireResult {
  GPUPresenterStatus status = GPUPresenterStatus::kError;
  std::unique_ptr<Surface> surface = nullptr;
};

/**
 * @class GPUPresenter
 *
 * The legacy GPUPresenter, RAII-wrapped over the neutral presenter
 * entries: acquires one-shot render surfaces and presents them. Only one
 * surface may be outstanding; present it before acquiring again. Created
 * through Context::CreatePresenter (the create info's p_next extension
 * selects the backend — Vulkan today). Resize / GetWidth / GetHeight are
 * C-side additions without legacy counterparts (the legacy presenter had
 * to be recreated on size changes); GetPresentMode has no C entry and is
 * omitted.
 *
 */
class GPUPresenter
    : public detail::OwnHandle<skity_presenter, skity_presenter_destroy> {
 public:
  /**
   * Acquire the next one-shot render surface. On kSuccess / kNeedRecreate
   * retry after a Resize when the window changed; the surface is empty on
   * non-success.
   */
  GPUSurfaceAcquireResult AcquireNextSurface(
      const GPUSurfaceAcquireDescriptor& desc) {
    GPUSurfaceAcquireResult result;
    skity_surface handle = nullptr;
    skity_result raw = skity_presenter_acquire_next_surface(
        get(), desc.sample_count, desc.content_scale, &handle);
    result.status = presenter_status_of(raw);
    if (raw == SKITY_SUCCESS) {
      result.surface = std::unique_ptr<Surface>(new Surface(handle));
    }
    return result;
  }

  /**
   * Present an acquired surface (the legacy Present signature). On
   * kSuccess and kNeedRecreate the C layer consumes the handle; on other
   * statuses the surface keeps its handle.
   */
  GPUPresenterStatus Present(std::unique_ptr<Surface> surface) {
    if (surface == nullptr || !*surface) {
      return GPUPresenterStatus::kError;
    }
    skity_surface handle = surface->get();
    skity_result raw = skity_presenter_present(get(), &handle);
    if (raw == SKITY_SUCCESS || raw == SKITY_ERROR_NEED_RECREATE) {
      (void)surface->release();
    }
    return presenter_status_of(raw);
  }

  /** Presenter width in pixels (C-side addition). */
  uint32_t GetWidth() const { return skity_presenter_get_width(get()); }

  /** Presenter height in pixels (C-side addition). */
  uint32_t GetHeight() const { return skity_presenter_get_height(get()); }

  /** Recreate the swap resources for a new physical size (C-side
   *  addition). */
  skity_result Resize(uint32_t width, uint32_t height) {
    return skity_presenter_resize(get(), width, height);
  }

  /** Wrap a handle created through the raw C API. */
  static GPUPresenter Adopt(skity_presenter handle) {
    return GPUPresenter(handle);
  }

  /** Empty presenter (e.g. awaiting a CreatePresenter result). */
  GPUPresenter() = default;

 private:
  explicit GPUPresenter(skity_presenter h) : OwnHandle(h) {}
  friend class Context;
};

inline std::unique_ptr<GPUPresenter> Context::CreatePresenter(
    const skity_presenter_create_info& info) const {
  skity_presenter handle = nullptr;
  if (skity_presenter_create(get(), &info, &handle) != SKITY_SUCCESS) {
    return nullptr;
  }
  return std::unique_ptr<GPUPresenter>(new GPUPresenter(handle));
}

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_PRESENTER_HPP
