// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXTURE_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXTURE_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_context.h>
#include <skity_c/skity_types.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to a skity::Texture, a GPU image resource owned by a
 *         GPUContext that can be uploaded to or wrapped around an existing
 *         backend texture. */
SKITY_C_DEFINE_HANDLE(skity_texture);

/** @brief Texture pixel format. Values aligned with skity::TextureFormat. */
typedef enum {
  SKITY_TEXTURE_FORMAT_R = 0,  /**< single-channel (alpha/luminance) 8-bit */
  SKITY_TEXTURE_FORMAT_RGB,    /**< 3-channel, 8 bits per channel */
  SKITY_TEXTURE_FORMAT_RGB565, /**< 16-bit packed RGB (5-6-5) */
  SKITY_TEXTURE_FORMAT_RGBA,   /**< 4-channel, 8 bits per channel */
  SKITY_TEXTURE_FORMAT_BGRA,   /**< 4-channel, 8 bits per channel, B-order */
  SKITY_TEXTURE_FORMAT_S,      /**< single-channel special-purpose format */
} skity_texture_format;

/**
 * @brief Create a GPU texture owned by @p context. The texture is allocated on
 *        the context's backend; upload pixels with skity_texture_upload (or
 *        skity_texture_deferred_upload to upload lazily).
 *
 * @param context     owning context
 * @param format      pixel format of the allocation
 * @param width       texture width in pixels
 * @param height      texture height in pixels
 * @param alpha_type  alpha representation of the texture contents
 * @return new texture handle, or NULL on failure
 */
SKITY_C_API skity_texture skity_texture_create(skity_context context,
                                               skity_texture_format format,
                                               uint32_t width, uint32_t height,
                                               skity_alpha_type alpha_type);

/** @brief Release the texture handle and its GPU resource. Safe on NULL. */
SKITY_C_API void skity_texture_destroy(skity_texture texture);

/**
 * @brief Upload pixel data to the texture immediately. @p pixels is copied
 *        into an internal buffer before the upload, so the caller's buffer may
 *        be released right away.
 *
 * @param pixels      source pixel data, laid out per @p row_bytes
 * @param row_bytes   stride in bytes of each row in @p pixels
 * @param color_type  color type of @p pixels
 * @param alpha_type  alpha type of @p pixels
 */
SKITY_C_API void skity_texture_upload(skity_texture texture, const void* pixels,
                                      size_t row_bytes,
                                      skity_color_type color_type,
                                      skity_alpha_type alpha_type);

/**
 * @brief Store pixel data for a deferred upload. The actual GPU upload happens
 *        later, when the texture is first used by the rendering pipeline.
 *
 * @param pixels      source pixel data (copied), laid out per @p row_bytes
 * @param row_bytes   stride in bytes of each row in @p pixels
 * @param color_type  color type of @p pixels
 * @param alpha_type  alpha type of @p pixels
 */
SKITY_C_API void skity_texture_deferred_upload(skity_texture texture,
                                               const void* pixels,
                                               size_t row_bytes,
                                               skity_color_type color_type,
                                               skity_alpha_type alpha_type);

/** @brief Return the texture width in pixels. */
SKITY_C_API uint32_t skity_texture_get_width(skity_texture texture);

/** @brief Return the texture height in pixels. */
SKITY_C_API uint32_t skity_texture_get_height(skity_texture texture);

/**
 * @brief Descriptor for wrapping an externally-created GPU texture. Values
 *        mirror skity::GPUBackendTextureInfo. Set @p p_next to a
 *        backend-specific extension (e.g. skity_backend_texture_info_gl) to
 *        identify the texture.
 */
typedef struct skity_backend_texture_info {
  skity_structure_type s_type; /**< SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO */
  const void* p_next;          /**< → backend-specific extension (e.g. _gl) */
  skity_gpu_backend_type backend; /**< which backend owns the wrapped texture */
  uint32_t width;                 /**< texture width in pixels */
  uint32_t height;                /**< texture height in pixels */
  skity_texture_format format;    /**< pixel format of the wrapped texture */
  skity_alpha_type alpha_type;    /**< alpha representation of the contents */
} skity_backend_texture_info;

/**
 * @brief GL backend extension: wraps an existing GL texture id. Chain via
 *        skity_backend_texture_info::p_next with s_type =
 *        SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_GL.
 */
typedef struct skity_backend_texture_info_gl {
  skity_structure_type
      s_type;               /**< SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_GL */
  const void* p_next;       /**< reserved for future extensions */
  uint32_t texture_id;      /**< existing GL texture id to wrap */
  uint32_t owned_by_engine; /**< non-zero if skity owns and must delete it */
} skity_backend_texture_info_gl;

/**
 * @brief Release callback invoked when skity no longer references a wrapped
 *        backend texture.
 *
 * @param userdata opaque pointer registered with the wrap call
 */
typedef void (*skity_texture_release_callback)(void* userdata);

/**
 * @brief Wrap an existing backend texture (e.g. a GL texture id created
 *        elsewhere) as a skity texture.
 *
 * @param context  owning context
 * @param info     backend texture descriptor (with extension chained via
 *                 p_next)
 * @param release  invoked (with @p userdata) when skity drops its reference;
 *                 may be NULL
 * @param userdata opaque pointer passed to @p release
 * @return new texture handle, or NULL on failure
 */
SKITY_C_API skity_texture skity_texture_create_from_backend(
    skity_context context, const skity_backend_texture_info* info,
    skity_texture_release_callback release, void* userdata);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXTURE_H
