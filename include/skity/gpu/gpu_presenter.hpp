// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_PRESENTER_HPP
#define INCLUDE_SKITY_GPU_GPU_PRESENTER_HPP

#include <memory>
#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * @brief Descriptor for creating a GPU presenter.
 *
 * @details `width` and `height` describe the physical presentation size in
 *          pixels. For swapchain-backed presenters, these values typically map
 *          to the swapchain extent.
 */
struct GPUPresenterDescriptor {
  GPUBackendType backend = GPUBackendType::kNone;

  /**
   * Physical presentation width in pixels.
   */
  uint32_t width = 0;

  /**
   * Physical presentation height in pixels.
   */
  uint32_t height = 0;
};

/**
 * @brief Descriptor for acquiring a renderable surface from a presenter.
 *
 * @details The acquired surface logical size is derived from the presenter
 *          physical size and `content_scale`:
 *          `logical_size * content_scale == physical_size`.
 */
struct GPUSurfaceAcquireDescriptor {
  /**
   * Sample count used when rendering this acquired surface.
   */
  uint32_t sample_count = 1;

  /**
   * Logical-to-physical scale factor for the acquired surface.
   */
  float content_scale = 1.f;
};

/**
 * @brief Result status for presenter acquire/present operations.
 */
enum class GPUPresenterStatus {
  /**
   * Operation succeeded.
   */
  kSuccess,

  /**
   * The current swapchain is no longer compatible with presentation and should
   * be recreated by the caller.
   */
  kNeedRecreate,

  /**
   * Operation failed for a reason other than swapchain recreation.
   */
  kError,
};

/**
 * @brief Result returned by `AcquireNextSurface()`.
 */
struct GPUSurfaceAcquireResult {
  /**
   * Outcome of the acquire operation.
   */
  GPUPresenterStatus status = GPUPresenterStatus::kError;

  /**
   * One-shot surface returned on successful acquire.
   */
  std::unique_ptr<GPUSurface> surface = nullptr;
};

/**
 * @brief GPU backend presenter for presenting the surface on screen.
 *
 * @details The presenter is responsible for acquiring the next surface from the
 *          GPU and presenting it on the screen.
 *          The presenter is responsible for managing the surface lifecycle.
 *
 * @note This API is experimental. Might change in the future. Currently only
 *       Vulkan backend supports this.
 *
 * @note The presenter is immutable. If the window size changes, the presenter
 *       must be recreated.
 *
 */
class SKITY_API GPUPresenter {
 public:
  virtual ~GPUPresenter() = default;

  /**
   * @brief Acquire the next surface from the GPU.
   *
   * @param desc describe per-surface rendering parameters such as sample count
   *             and content scale. The acquired surface logical size is
   *             derived from the presenter physical size and `content_scale`.
   *
   * @return A structured acquire result. On success, `surface` contains a
   *         one-shot GPUSurface instance. The returned surface should be
   *         presented or discarded before acquiring the next one from the same
   *         presenter.
   */
  virtual GPUSurfaceAcquireResult AcquireNextSurface(
      const GPUSurfaceAcquireDescriptor& desc) = 0;

  /**
   * @brief Present a surface previously acquired from this presenter.
   *
   * @param surface one-shot surface returned by `AcquireNextSurface()`
   * @return Presentation status. `kNeedRecreate` indicates that the caller
   *         should recreate the presenter and its underlying swapchain.
   */
  virtual GPUPresenterStatus Present(std::unique_ptr<GPUSurface> surface) = 0;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_PRESENTER_HPP
