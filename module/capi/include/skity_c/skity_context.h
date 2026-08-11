// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_H

#include <skity_c/skity_base.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to a skity::GPUContext, the root of the GPU backend
 *         that owns surfaces, textures, and the render-thread state. */
SKITY_C_DEFINE_HANDLE(skity_context);

/**
 * @brief GL procedure address loader.
 *
 * A plain C function pointer `void* (*)(const char*)`, matching the glad
 * GLADloadfunc contract that skity::GLContextCreate expects internally.
 * Pass something like glfwGetProcAddress / eglGetProcAddress / the platform
 * equivalent.
 */
typedef void* (*skity_gl_get_proc)(const char* name);

/**
 * @brief Create a GPUContext targeting the OpenGL / OpenGL ES backend.
 *
 * The created context owns its GL interface and is returned with the owning
 * flag set; call skity_context_destroy to release it.
 *
 * @param get_proc    function used to load GL symbol addresses at runtime
 * @param out_context receives the new context handle on success
 * @return SKITY_SUCCESS on success, or a SKITY_ERROR_* code on failure
 */
SKITY_C_API skity_result skity_context_create_gl(skity_gl_get_proc get_proc,
                                                 skity_context* out_context);

/** @brief Destroy a context previously created by skity_context_create_gl or
 *         skity_context_create_vk. Safe to call with NULL or a wrong-type
 *         handle (both are rejected silently). */
SKITY_C_API void skity_context_destroy(skity_context context);

/** @brief GPU error code reported through the error callback. Values aligned
 *         with skity::GPUError. */
typedef enum {
  SKITY_GPU_ERROR_NO_ERROR = 0,   /**< no error, everything is fine */
  SKITY_GPU_ERROR_GPU_ERROR,      /**< error during GPU context creation (e.g.
                                     missing driver) */
  SKITY_GPU_ERROR_PIPELINE_ERROR, /**< error during pipeline creation (shader
                                     compile/link) */
} skity_gpu_error;

/**
 * @brief Error callback invoked by the engine. skity_gpu_error is
 *        ABI-compatible with skity::GPUError (both int-backed enums with
 *        matching values), so the C callback is passed straight through.
 *
 * @param error    error code
 * @param message  human-readable description (valid only for the duration of
 *                 the call)
 * @param userdata opaque pointer registered alongside the callback
 */
typedef void (*skity_gpu_error_callback)(skity_gpu_error error,
                                         const char* message, void* userdata);

/**
 * @brief Register a callback to receive engine error reports.
 * @param callback  function invoked on error (may be NULL to clear)
 * @param userdata  opaque pointer passed back to the callback
 */
SKITY_C_API void skity_context_set_error_callback(
    skity_context context, skity_gpu_error_callback callback, void* userdata);

/** @brief Control whether draw calls that can be merged are merged internally
 *         (on by default).
 * @param enable non-zero to enable */
SKITY_C_API void skity_context_set_enable_merging_draw_call(
    skity_context context, uint32_t enable);

/** @brief Control whether contour-based AA is used for anti-aliasing when MSAA
 *         is disabled (off by default).
 * @param enable non-zero to enable */
SKITY_C_API void skity_context_set_enable_contour_aa(skity_context context,
                                                     uint32_t enable);

/** @brief Set the default Coverage AA state for surfaces whose render option is
 *         kAuto (off by default).
 * @param enable non-zero to enable */
SKITY_C_API void skity_context_set_enable_coverage_aa(skity_context context,
                                                      uint32_t enable);

/** @brief Set the default sampling-based conflation correction heuristic for
 *         surfaces whose render option is kAuto. Ignored when Coverage AA is
 *         disabled (off by default).
 * @param enable non-zero to enable */
SKITY_C_API void skity_context_set_conflation_correction(skity_context context,
                                                         uint32_t enable);

/**
 * @brief Use a larger atlas cache for better performance at the cost of memory
 *        (4x per set bit).
 * @param mask  bit 0 = A8 atlas (normal text), bit 1 = RGBA32 atlas (emoji)
 */
SKITY_C_API void skity_context_set_larger_atlas_mask(skity_context context,
                                                     uint8_t mask);

/**
 * @brief Use linear filtering when sampling text. Skity does not currently
 *        render rotated text, so linear filtering hides jagged edges.
 * @warning This is a workaround and will be removed in the next major version.
 * @param enable non-zero to enable
 */
SKITY_C_API void skity_context_set_enable_text_linear_filter(
    skity_context context, uint32_t enable);

/** @brief Control whether the GPU tessellation path is used (on by default).
 * @param enable non-zero to enable */
SKITY_C_API void skity_context_set_enable_gpu_tessellation(
    skity_context context, uint32_t enable);

/** @brief Control whether the simple-shape pipeline is used (off by default).
 * @param enable non-zero to enable */
SKITY_C_API void skity_context_set_enable_simple_shape_pipeline(
    skity_context context, uint32_t enable);

/**
 * @brief Set the maximum GPU resource cache size in bytes. Pass 0 to disable
 *        the cache. Mirrors GPUContext::SetResourceCacheLimit (experimental).
 * @param max_bytes  maximum cache size in bytes, or 0 to disable caching
 */
SKITY_C_API void skity_context_set_resource_cache_limit(skity_context context,
                                                        size_t max_bytes);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_H
