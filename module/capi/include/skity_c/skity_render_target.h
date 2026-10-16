// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_RENDER_TARGET_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_RENDER_TARGET_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_canvas.h>
#include <skity_c/skity_context.h>
#include <skity_c/skity_image.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

SKITY_C_DEFINE_HANDLE(skity_render_target);

/**
 * @brief Coverage-AA selection for an offscreen render target, mirroring
 *        skity::CoverageAAMode.
 */
typedef enum {
  SKITY_COVERAGE_AA_AUTO = 0,              /**< inherit the context default */
  SKITY_COVERAGE_AA_DISABLED,              /**< disable coverage AA */
  SKITY_COVERAGE_AA_ANALYTICAL,            /**< analytical coverage AA without
                                                conflation correction */
  SKITY_COVERAGE_AA_CONFLATION_CORRECTION, /**< analytical coverage AA with a
                                                sampling heuristic for selected
                                                conflation patterns (costs
                                                additional fragment work) */
} skity_coverage_aa_mode;

/**
 * @brief Offscreen render-target creation descriptor, mirroring
 *        skity::GPURenderTargetDescriptor. The pixel format is decided by
 *        the context (RGBA8); a render target can not be reused after
 *        snapshotting.
 */
typedef struct skity_render_target_descriptor {
  uint32_t width;                     /**< pixel width (> 0) */
  uint32_t height;                    /**< pixel height (> 0) */
  uint32_t sample_count;              /**< MSAA sample count (1 = no MSAA) */
  skity_coverage_aa_mode coverage_aa; /**< coverage-AA selection */
} skity_render_target_descriptor;

/**
 * @brief Create an offscreen render target on @p context's backend.
 *
 * Draws issued through the canvas returned by
 * skity_render_target_get_canvas are recorded and replayed onto the target
 * when it is snapshot with skity_context_make_snapshot.
 *
 * @param context  owning GPU context
 * @param desc     creation descriptor
 * @return new render-target handle, or NULL on failure
 */
SKITY_C_API skity_render_target skity_context_create_render_target(
    skity_context context, const skity_render_target_descriptor* desc);

/**
 * @brief Return the recording canvas of the render target.
 *
 * The returned handle is a non-owning view: it stays valid until the render
 * target is snapshot (skity_context_make_snapshot) or destroyed.
 *
 * @param target render-target handle
 * @return canvas view handle, or NULL on an invalid handle
 */
SKITY_C_API skity_canvas
skity_render_target_get_canvas(skity_render_target target);

/** @brief Return the pixel width of the render target. */
SKITY_C_API uint32_t skity_render_target_get_width(skity_render_target target);

/** @brief Return the pixel height of the render target. */
SKITY_C_API uint32_t skity_render_target_get_height(skity_render_target target);

/**
 * @brief Flush the pending draw calls held by @p target and produce an image
 *        of its contents.
 *
 * Consumes @p target: the handle (and any canvas view obtained from it)
 * becomes invalid and must not be used or destroyed afterwards.
 *
 * @param context  the context the render target was created on
 * @param target   render-target handle to consume
 * @return image handle of the snapshot contents, or NULL on failure
 */
SKITY_C_API skity_image skity_context_make_snapshot(skity_context context,
                                                    skity_render_target target);

/** @brief Release the render-target handle. Safe on NULL. */
SKITY_C_API void skity_render_target_destroy(skity_render_target target);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_RENDER_TARGET_H
