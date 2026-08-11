// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_BITMAP_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_BITMAP_H

#include <skity_c/skity_base.h>
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
 * @brief Read-only pixel view, mirroring skity::Pixmap. Typically obtained from
 *        skity_image_read_pixels; the caller must not write through the pixel
 *        pointer.
 */
SKITY_C_DEFINE_HANDLE(skity_pixmap);

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
 * @brief Borrow the writable pixel view of a bitmap. The returned pixmap
 *        shares the bitmap's pixel memory (writes through it are visible to
 *        the bitmap), so it can be used as a destination for e.g.
 *        skity_image_scale_pixels. Destroy with skity_pixmap_destroy; the
 *        bitmap keeps its pixels.
 * @return a new pixmap handle, or NULL on failure
 */
SKITY_C_API skity_pixmap skity_bitmap_get_pixmap(skity_bitmap bitmap);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_BITMAP_H
