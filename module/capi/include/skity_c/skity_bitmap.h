// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_BITMAP_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_BITMAP_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_data.h>
#include <skity_c/skity_types.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CPU-side pixel buffer the caller owns and may read/write, mirroring
 *        skity::Bitmap. Allocated with the requested color and alpha types and
 *        backed by a Pixmap.
 */
SKITY_C_DEFINE_HANDLE(skity_bitmap);

/**
 * @brief Allocate a new bitmap of @p width x @p height pixels with the given
 *        pixel formats.
 * @param width       pixel width
 * @param height      pixel height
 * @param alpha_type  alpha interpretation of the pixels
 * @param color_type  color packing of the pixels
 * @return            a new handle, or NULL on failure
 */
SKITY_C_API skity_bitmap skity_bitmap_create(uint32_t width, uint32_t height,
                                             skity_alpha_type alpha_type,
                                             skity_color_type color_type);

/** @brief Release the bitmap handle and its pixel storage. Safe on NULL. */
SKITY_C_API void skity_bitmap_destroy(skity_bitmap bitmap);

/** @brief Return the pixel width of the bitmap. */
SKITY_C_API uint32_t skity_bitmap_get_width(skity_bitmap bitmap);

/** @brief Return the pixel height of the bitmap. */
SKITY_C_API uint32_t skity_bitmap_get_height(skity_bitmap bitmap);

/** @brief Return the row stride in bytes (may be larger than width*bpp). */
SKITY_C_API size_t skity_bitmap_get_row_bytes(skity_bitmap bitmap);

/**
 * @brief Return a writable pointer to the pixel data. The memory stays valid
 *        for the life of the bitmap handle.
 * @return pointer to the writable pixels
 */
SKITY_C_API void* skity_bitmap_get_pixels(skity_bitmap bitmap);

/**
 * @brief Pixel view over a buffer, mirroring skity::Pixmap. Either created
 *        wrapping a skity_data buffer, or borrowed from an image read / a
 *        bitmap; the pixel pointer is exposed read-only.
 */
SKITY_C_DEFINE_HANDLE(skity_pixmap);

/**
 * @brief Wrap an existing data buffer as a pixmap without copying.
 *
 * The pixmap keeps a reference to @p data, so the buffer stays alive even if
 * the caller drops the data handle. @p row_bytes may exceed
 * width * bytes-per-pixel (strided sources). No validation is performed on
 * the buffer size — the caller guarantees @p data holds at least
 * @p row_bytes * @p height bytes.
 *
 * @param data        buffer holding the pixels
 * @param row_bytes   row stride in bytes
 * @param width       pixel width
 * @param height      pixel height
 * @param alpha_type  alpha interpretation of the pixels
 * @param color_type  color packing of the pixels
 * @return            a new handle, or NULL on failure
 */
SKITY_C_API skity_pixmap skity_pixmap_create(skity_data data, size_t row_bytes,
                                             uint32_t width, uint32_t height,
                                             skity_alpha_type alpha_type,
                                             skity_color_type color_type);

/** @brief Release the pixmap handle. Safe on NULL. */
SKITY_C_API void skity_pixmap_destroy(skity_pixmap pixmap);

/** @brief Return the pixel width of the pixmap. */
SKITY_C_API uint32_t skity_pixmap_get_width(skity_pixmap pixmap);

/** @brief Return the pixel height of the pixmap. */
SKITY_C_API uint32_t skity_pixmap_get_height(skity_pixmap pixmap);

/** @brief Return the row stride in bytes (may be larger than width*bpp). */
SKITY_C_API size_t skity_pixmap_get_row_bytes(skity_pixmap pixmap);

/**
 * @brief Return a read-only pointer to the pixel data.
 * @return pointer to the immutable pixels
 */
SKITY_C_API const void* skity_pixmap_get_pixels(skity_pixmap pixmap);

/**
 * @brief Update the alpha and color interpretation of the pixels.
 *
 * Use this when the encoding of the already-wrapped buffer changed (e.g. a
 * decoder produced premultiplied output after the pixmap was created).
 *
 * @param pixmap      the pixmap to update
 * @param alpha_type  new alpha interpretation
 * @param color_type  new color packing
 * @return            SKITY_SUCCESS, or SKITY_ERROR_INVALID_ARGUMENT when the
 *                    combination is rejected
 */
SKITY_C_API skity_result
skity_pixmap_set_color_info(skity_pixmap pixmap, skity_alpha_type alpha_type,
                            skity_color_type color_type);

/**
 * @brief Borrow the writable pixel view of a bitmap. The returned pixmap
 *        shares the bitmap's pixel memory (writes through it are visible to
 *        the bitmap), so it can be used as a destination for e.g.
 *        skity_image_scale_pixels. Destroy with skity_pixmap_destroy; the
 *        bitmap keeps its pixels.
 * @return a new pixmap handle, or NULL on failure
 */
SKITY_C_API skity_pixmap skity_bitmap_get_pixmap(skity_bitmap bitmap);

/**
 * @brief Wrap an existing pixmap as a bitmap without copying.
 *
 * The bitmap shares the pixmap's pixel memory. Pass a non-zero @p read_only
 * to mark it immutable; pass 0 when the pixels will be written through the
 * bitmap (e.g. a software rendering target).
 *
 * @param pixmap    pixel source to wrap (it keeps the data alive via shared
 *                  ownership, so the buffer outlives the bitmap)
 * @param read_only non-zero to mark the bitmap read-only
 * @return          a new handle, or NULL on failure
 */
SKITY_C_API skity_bitmap skity_bitmap_create_from_pixmap(skity_pixmap pixmap,
                                                         uint32_t read_only);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_BITMAP_H
