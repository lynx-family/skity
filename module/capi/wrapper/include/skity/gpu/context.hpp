// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_CONTEXT_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_CONTEXT_HPP

#include <skity/c/skity_context.h>
#include <skity/c/skity_presenter.h>
#include <skity/c/skity_surface.h>
#include <skity/c/skity_texture.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <skity/base.hpp>
#include <skity/types.hpp>

namespace skity {

class Surface;
class Texture;
class Semaphore;
class GPUPresenter;
namespace detail {
struct ContextCreate;
}  // namespace detail

/**
 * @class Context
 *
 * GPU context wrapper: root of the GPU backend, owns surfaces.
 *
 */
class Context : public detail::OwnHandle<skity_context, skity_context_destroy> {
 public:
  /** Empty context — placeholder for optional-context parameters. */
  Context() = default;

  /**
   * Create an OpenGL / OpenGL ES context (the legacy GLContextCreate
   * shape). get_proc loads GL symbol addresses (glad GLADloadfunc
   * contract); a live GL context is required. Null on failure.
   */
  static std::unique_ptr<Context> CreateGL(skity_gl_get_proc get_proc) {
    skity_context handle = nullptr;
    if (skity_context_create_gl(get_proc, &handle) != SKITY_SUCCESS) {
      return nullptr;
    }
    return std::unique_ptr<Context>(new Context(handle));
  }

  /**
   * Adopt a context created through the raw C API — the bridge for
   * backends whose creation entry points stay unwrapped by design (e.g.
   * skity_context_create_vk, whose signature carries Vulkan types). The
   * wrapper takes ownership; destroying it releases the context.
   */
  static Context Adopt(skity_context handle) { return Context(handle); }

  /** Register a callback receiving engine error reports (NULL clears). */
  void SetErrorCallback(skity_gpu_error_callback callback, void* userdata) {
    skity_context_set_error_callback(get(), callback, userdata);
  }

  /** Merge compatible draw calls internally (on by default). */
  void SetEnableMergingDrawCall(bool enable) {
    skity_context_set_enable_merging_draw_call(get(), enable ? 1u : 0u);
  }

  /** Contour-based AA when MSAA is disabled (off by default). */
  void SetEnableContourAA(bool enable) {
    skity_context_set_enable_contour_aa(get(), enable ? 1u : 0u);
  }

  /** Coverage-based AA path (off by default). */
  void SetEnableCoverageAA(bool enable) {
    skity_context_set_enable_coverage_aa(get(), enable ? 1u : 0u);
  }

  /** GPU tessellation of geometry (off by default). */
  void SetEnableGPUTessellation(bool enable) {
    skity_context_set_enable_gpu_tessellation(get(), enable ? 1u : 0u);
  }

  /** Simple-shape pipeline (off by default). */
  void SetEnableSimpleShapePipeline(bool enable) {
    skity_context_set_enable_simple_shape_pipeline(get(), enable ? 1u : 0u);
  }

  /** Linear filtering when sampling text (off by default). */
  void SetEnableTextLinearFilter(bool enable) {
    skity_context_set_enable_text_linear_filter(get(), enable ? 1u : 0u);
  }

  /** Conflation correction for overlapping geometry (off by default). */
  void SetConflationCorrection(bool enable) {
    skity_context_set_conflation_correction(get(), enable ? 1u : 0u);
  }

  /**
   * Larger atlas caches for better performance at the cost of memory
   * (4x per set bit): bit 0 = A8 atlas (normal text), bit 1 = RGBA32 (emoji).
   */
  void SetLargerAtlasMask(uint8_t mask) {
    skity_context_set_larger_atlas_mask(get(), mask);
  }

  /** Maximum GPU resource cache size in bytes; 0 disables the cache. */
  void SetResourceCacheLimit(size_t max_bytes) {
    skity_context_set_resource_cache_limit(get(), max_bytes);
  }

  /**
   * The legacy GPUContext hub members (the C++ API's CreateSurface /
   * CreateTexture / CreateTextureWithDesc / WrapTexture / CreateSemaphore).
   * Their parameters use the C ABI's own descriptor structs — the C layer
   * replaced the legacy GPUSurfaceDescriptor / GPUBackendTextureInfo with
   * the p_next-chained CreateInfo families. Declared here, defined in
   * gpu/surface.hpp / gpu/texture.hpp / gpu/semaphore.hpp — include the
   * matching header to call them (the skity.hpp umbrella already does).
   */
  std::unique_ptr<Surface> CreateSurface(
      const skity_surface_create_info& info) const;
  std::shared_ptr<Texture> CreateTexture(skity_texture_format format,
                                         uint32_t width, uint32_t height,
                                         AlphaType alpha_type) const;
  std::shared_ptr<Texture> CreateTextureWithDesc(
      const skity_texture_descriptor& desc) const;
  std::shared_ptr<Texture> WrapTexture(
      const skity_backend_texture_info& info,
      skity_texture_release_callback release = nullptr,
      void* userdata = nullptr) const;
  std::shared_ptr<Semaphore> CreateSemaphore() const;
  std::unique_ptr<GPUPresenter> CreatePresenter(
      const skity_presenter_create_info& info) const;

  /** GPU backend classification, mirroring skity::GPUBackendType. */
  enum class BackendType : uint32_t {
    kNone = SKITY_GPU_BACKEND_TYPE_NONE,
    kOpenGL = SKITY_GPU_BACKEND_TYPE_OPENGL,
    kVulkan = SKITY_GPU_BACKEND_TYPE_VULKAN,
    kWebGL2 = SKITY_GPU_BACKEND_TYPE_WEBGL2,
    kWebGPU = SKITY_GPU_BACKEND_TYPE_WEBGPU,
    kMetal = SKITY_GPU_BACKEND_TYPE_METAL,
  };

  /** Whether skity was compiled with support for type. */
  static bool IsGPUSupported(BackendType type) {
    return skity_is_gpu_backend_supported(
               static_cast<skity_gpu_backend_type>(type)) != 0u;
  }

  /** The backend this context was created with. */
  BackendType GetBackendType() const {
    return static_cast<BackendType>(skity_context_get_backend_type(get()));
  }

 private:
  explicit Context(skity_context h) : OwnHandle(h) {}
  friend struct detail::ContextCreate;
};

namespace detail {
/** Internal: backend creation helpers (skity_vk / mtl headers) construct
 *  through here — friending those free functions directly would drag
 *  Vulkan / Objective-C types into this neutral header. */
struct ContextCreate {
  static std::unique_ptr<Context> New(skity_context h) {
    return std::unique_ptr<Context>(new Context(h));
  }
};
}  // namespace detail

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_CONTEXT_HPP
