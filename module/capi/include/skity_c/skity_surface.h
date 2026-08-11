// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SURFACE_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SURFACE_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_bitmap.h>
#include <skity_c/skity_context.h>
#include <skity_c/skity_types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to a skity::GPUSurface, a GPU-backed render target
 *         that owns a canvas and drives per-frame drawing. */
SKITY_C_DEFINE_HANDLE(skity_surface);

/** @brief Opaque handle to the skity::Canvas associated with a surface. The
 *         surface owns the canvas; this handle is non-owning. */
SKITY_C_DEFINE_HANDLE(skity_canvas);

/** @brief Structure type values (skity_structure_type) and the backend
 *         extension structs they tag are defined in skity_c/skity_types.h so
 *         they can be shared across surface and texture. */

/**
 * @brief Base surface creation descriptor. Values mirror
 *        skity::GPUSurfaceDescriptor. Set @p p_next to point at a
 *        backend-specific extension (e.g. skity_surface_create_info_gl) to
 *        select the render target.
 */
typedef struct skity_surface_create_info {
  skity_structure_type s_type; /**< SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO */
  const void* p_next;          /**< → backend-specific extension struct */
  uint32_t width;              /**< surface width in pixels */
  uint32_t height;             /**< surface height in pixels */
  uint32_t sample_count;       /**< MSAA sample count (1 = no MSAA) */
  float content_scale;         /**< logical-to-physical pixel scale */
} skity_surface_create_info;

/** @brief GL surface target kind. */
typedef enum {
  SKITY_GL_SURFACE_TYPE_INVALID = 0, /**< unused / sentinel */
  SKITY_GL_SURFACE_TYPE_TEXTURE,     /**< render into a GL texture */
  SKITY_GL_SURFACE_TYPE_FRAMEBUFFER, /**< render into a GL framebuffer object */
} skity_gl_surface_type;

/**
 * @brief How a framebuffer-backed GL surface presents its final color contents
 *        to the target framebuffer. Values align with skity::GLSurfaceMode.
 *        Ignored when surface_type is not SKITY_GL_SURFACE_TYPE_FRAMEBUFFER.
 */
typedef enum {
  SKITY_GL_SURFACE_MODE_AUTO = 0, /**< let the backend choose */
  SKITY_GL_SURFACE_MODE_DIRECT,   /**< render directly into the target FBO */
  SKITY_GL_SURFACE_MODE_BLIT,     /**< render offscreen, blit to target FBO */
  SKITY_GL_SURFACE_MODE_DRAW_TEXTURE, /**< render offscreen, draw as texture */
} skity_gl_surface_mode;

/**
 * @brief GL backend extension. Hang this off skity_surface_create_info::p_next
 *        with s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_GL.
 */
typedef struct skity_surface_create_info_gl {
  skity_structure_type
      s_type;         /**< SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_GL */
  const void* p_next; /**< reserved for future extensions */
  skity_gl_surface_type
      surface_type; /**< whether @p gl_id is a texture or FBO */
  uint32_t
      gl_id; /**< GL texture id or framebuffer id (0 = on-screen default FBO) */
  uint32_t has_stencil; /**< non-zero if the target has a stencil attachment
                           (FRAMEBUFFER only) */
  skity_gl_surface_mode surface_mode; /**< how a FBO surface presents to the
                                         target (FRAMEBUFFER only); defaults to
                                         SKITY_GL_SURFACE_MODE_AUTO */
  uint32_t can_blit_from_target_fbo;  /**< non-zero to blit from the target FBO
                                         to the internal FBO before drawing
                                         (FRAMEBUFFER only, no stencil,
                                         sample_count == 1) */
} skity_surface_create_info_gl;

/**
 * @brief Create a GPU surface bound to the given context. The backend is
 *        chosen by the p_next extension of @p info.
 * @param context     context that will own the surface
 * @param info        creation descriptor (with backend extension chained via
 *                    p_next)
 * @param out_surface receives the new surface handle on success
 * @return SKITY_SUCCESS on success, or a SKITY_ERROR_* code on failure
 */
SKITY_C_API skity_result skity_surface_create(
    skity_context context, const skity_surface_create_info* info,
    skity_surface* out_surface);

/** @brief Release the surface and its underlying GPU resources. Safe on NULL.
 */
SKITY_C_API void skity_surface_destroy(skity_surface surface);

/**
 * @brief Lock the canvas for the current frame.
 *
 * The returned canvas is owned by the surface (non-owning handle) and stays
 * valid until the next skity_surface_lock_canvas call or until the surface is
 * destroyed.
 *
 * @param clear  non-zero to clear the previous frame's contents; skipping the
 *               clear costs extra memory and may hurt performance
 * @return Canvas handle for the current frame (NULL on failure)
 */
SKITY_C_API skity_canvas skity_surface_lock_canvas(skity_surface surface,
                                                   uint32_t clear);

/** @brief Present the rendering result. Canvas::Flush must be called first. */
SKITY_C_API void skity_surface_flush(skity_surface surface);

/**
 * @brief Read a region of the rendered surface back into CPU memory.
 *
 * Mirrors GPUSurface::ReadPixels (experimental).
 *
 * @param rect region to read; NULL reads the full surface
 * @return a read-only skity_pixmap handle (use skity_pixmap_get_pixels to
 *         access the data and skity_pixmap_destroy to release it), or NULL on
 *         failure
 */
SKITY_C_API skity_pixmap skity_surface_read_pixels(skity_surface surface,
                                                   const skity_rect* rect);

/** @brief Return the surface width in pixels. */
SKITY_C_API uint32_t skity_surface_get_width(skity_surface surface);

/** @brief Return the surface height in pixels. */
SKITY_C_API uint32_t skity_surface_get_height(skity_surface surface);

/** @brief Return the logical-to-physical pixel content scale. */
SKITY_C_API float skity_surface_get_content_scale(skity_surface surface);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SURFACE_H
