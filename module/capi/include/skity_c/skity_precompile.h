// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PRECOMPILE_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PRECOMPILE_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_context.h>
#include <skity_c/skity_paint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to a skity::PrecompileContext, used to warm up GPU
 *         pipeline variants before they are needed for drawing. The context
 *         must be used on the same thread that owns its GPUContext. */
SKITY_C_DEFINE_HANDLE(skity_precompile_context);

/** @brief Target surface color type for precompilation. Values aligned with
 *         skity::PrecompileColorType. */
typedef enum {
  SKITY_PRECOMPILE_COLOR_TYPE_RGBA = 0, /**< 8-bit RGBA channel order */
  SKITY_PRECOMPILE_COLOR_TYPE_BGRA,     /**< 8-bit BGRA channel order */
} skity_precompile_color_type;

/** @brief Draw type for targeted precompilation. Values aligned with
 *         skity::PrecompileDrawType. */
typedef enum {
  SKITY_PRECOMPILE_DRAW_TYPE_RECT = 0,    /**< axis-aligned rectangle */
  SKITY_PRECOMPILE_DRAW_TYPE_RRECT,       /**< rounded rectangle */
  SKITY_PRECOMPILE_DRAW_TYPE_PATH,        /**< arbitrary path */
  SKITY_PRECOMPILE_DRAW_TYPE_TEXT,        /**< regular glyph text */
  SKITY_PRECOMPILE_DRAW_TYPE_SDF_TEXT,    /**< SDF-rendered text */
  SKITY_PRECOMPILE_DRAW_TYPE_EMOJI_TEXT,  /**< emoji / color-glyph text */
  SKITY_PRECOMPILE_DRAW_TYPE_IMAGE,       /**< image draw */
  SKITY_PRECOMPILE_DRAW_TYPE_IMAGE_RRECT, /**< image draw clipped to an rrect */
  SKITY_PRECOMPILE_DRAW_TYPE_CLIP_PATH,   /**< path used as a clip */
} skity_precompile_draw_type;

/** @brief Warm up a common set of graphics / image / text pipelines. The exact
 *         pipeline set is an implementation detail and may evolve over time. */
SKITY_C_API void skity_precompile_context_precompile_default_shaders(
    skity_precompile_context context);

/**
 * @brief Warm up the pipeline for a specific draw type + paint combination.
 * @param draw_type  which draw operation to target
 * @param paint      paint state whose pipeline variants should be precompiled
 */
SKITY_C_API void skity_precompile_context_precompile_draw(
    skity_precompile_context context, skity_precompile_draw_type draw_type,
    skity_paint paint);

/**
 * @brief Create a precompile context bound to @p context, used to warm up
 *        pipeline variants before drawing.
 *
 * @p color_type / @p enable_msaa should match the surface that will later
 * render with the precompiled shaders; when MSAA is enabled precompile uses a
 * sample count of 4.
 *
 * @param context     owning GPU context
 * @param color_type  color type of the target surface
 * @param enable_msaa non-zero if the target surface uses MSAA
 * @return new precompile context handle, or NULL on failure
 */
SKITY_C_API skity_precompile_context skity_context_create_precompile_context(
    skity_context context, skity_precompile_color_type color_type,
    uint32_t enable_msaa);

/** @brief Release the precompile context. Safe on NULL. */
SKITY_C_API void skity_precompile_context_destroy(
    skity_precompile_context context);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_PRECOMPILE_H
