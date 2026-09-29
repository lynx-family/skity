// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_HPP

// GPU context wrapper of the header-only RAII layer: root of the GPU
// backend, owns surfaces. See skity.hpp for the layer's design.

#include <skity_c/skity_context.h>

#include <cstddef>
#include <cstdint>
#include <skity_hpp/skity_base.hpp>

namespace skity {
namespace raii {

/** GPU context wrapper: root of the GPU backend, owns surfaces. */
class Context : public detail::OwnHandle<skity_context, skity_context_destroy> {
 public:
  /**
   * Create an OpenGL / OpenGL ES context. @p get_proc loads GL symbol
   * addresses (glad GLADloadfunc contract); a live GL context is required.
   * On failure @p out is left empty and the result code is returned.
   */
  static skity_result CreateGL(skity_gl_get_proc get_proc, Context* out) {
    skity_context handle = nullptr;
    skity_result result = skity_context_create_gl(get_proc, &handle);
    if (result == SKITY_SUCCESS && out != nullptr) {
      out->reset(handle);
    }
    return result;
  }

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
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_HPP
