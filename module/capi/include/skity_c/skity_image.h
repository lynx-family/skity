// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_IMAGE_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_IMAGE_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_bitmap.h>
#include <skity_c/skity_texture.h>
#include <skity_c/skity_types.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Image: a 2D raster of pixels that can be sampled by a shader or drawn
 *        to a surface. Backed by CPU memory, a GPU texture, a deferred-upload
 *        placeholder, or a promise texture. Mirrors skity::Image.
 */
SKITY_C_DEFINE_HANDLE(skity_image);

/**
 * @brief Create a raster image from raw pixel data. The pixels are copied, so
 *        the caller's buffer may be released immediately after this returns.
 * @param width       pixel width
 * @param height      pixel height
 * @param pixels      pointer to the pixel data
 * @param row_bytes   bytes per row; 0 means width * bytes-per-pixel (tightly
 *                    packed)
 * @param alpha_type  how to interpret the alpha channel
 * @param color_type  how pixel bits encode color (RGBA / BGRA / ...)
 * @return            image handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image skity_image_create_from_pixels(
    uint32_t width, uint32_t height, const void* pixels, size_t row_bytes,
    skity_alpha_type alpha_type, skity_color_type color_type);

/**
 * @brief Create a raster image from raw pixel data, bound to a GPU @p context.
 *        Variant of skity_image_create_from_pixels that associates the image
 *        with a GPU context so it can be uploaded / used as a texture on that
 *        device. The pixels are copied, so the caller's buffer may be released
 *        immediately after this returns. Passing a NULL @p context degrades to
 *        the pure-CPU path (equivalent to skity_image_create_from_pixels).
 * @param width       pixel width
 * @param height      pixel height
 * @param pixels      pointer to the pixel data
 * @param row_bytes   bytes per row; 0 means width * bytes-per-pixel (tightly
 *                    packed)
 * @param alpha_type  how to interpret the alpha channel
 * @param color_type  how pixel bits encode color (RGBA / BGRA / ...)
 * @param context     GPU context to bind the image to; may be NULL
 * @return            image handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image skity_image_create_from_pixels_with_context(
    uint32_t width, uint32_t height, const void* pixels, size_t row_bytes,
    skity_alpha_type alpha_type, skity_color_type color_type,
    skity_context context);

/** @brief Release the image handle and its underlying object. Safe on NULL. */
SKITY_C_API void skity_image_destroy(skity_image image);

/** @brief Return the pixel width of the image. */
SKITY_C_API uint32_t skity_image_get_width(skity_image image);

/** @brief Return the pixel height of the image. */
SKITY_C_API uint32_t skity_image_get_height(skity_image image);

/**
 * @brief Wrap an existing GPU texture as an image. The image references the
 *        texture, which must outlive the image.
 * @param texture  GPU texture to wrap
 * @return         image handle, or NULL on failure
 */
SKITY_C_API skity_image skity_image_create_from_texture(skity_texture texture);

/**
 * @brief Create a deferred-upload image: a placeholder that knows its format
 *        and size but has no GPU texture yet. Bind a texture later with
 *        skity_image_deferred_set_texture. Useful when the texture is uploaded
 *        lazily or produced on another thread.
 * @param format      texture format to reserve
 * @param width       pixel width
 * @param height      pixel height
 * @param alpha_type  alpha interpretation for the future texture
 * @return            image handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image
skity_image_create_deferred(skity_texture_format format, uint32_t width,
                            uint32_t height, skity_alpha_type alpha_type);

/**
 * @brief Bind a texture to a deferred image created with
 *        skity_image_create_deferred. Calling this on a non-deferred image is
 *        undefined.
 * @param image    deferred image handle
 * @param texture  texture to bind; must outlive the image
 */
SKITY_C_API void skity_image_deferred_set_texture(skity_image image,
                                                  skity_texture texture);

/**
 * @brief Promise-texture callback invoked when skity needs the GPU texture
 *        backing a promise image. Return a shared (ref-counted) texture
 *        handle; the caller keeps ownership of the texture lifecycle (video /
 *        camera frames, cross-process textures, ...). The returned handle is
 *        shared with skity, so it stays alive until both sides release it.
 *
 * @param userdata  caller-supplied pointer passed back unchanged
 * @return          current GPU texture
 */
typedef skity_texture (*skity_promise_texture_callback)(void* userdata);

/**
 * @brief Variant of skity_promise_texture_callback that also receives the GPU
 *        context the image is being used with, allowing context-specific
 *        texture selection.
 *
 * @param userdata  caller-supplied pointer passed back unchanged
 * @param context   GPU context the image is currently bound to
 * @return          current GPU texture
 */
typedef skity_texture (*skity_promise_texture_callback2)(void* userdata,
                                                         skity_context context);

/**
 * @brief Promise-texture callback invoked when a promise image is destroyed
 *        and the caller no longer needs to keep its texture alive.
 *
 * @param userdata  caller-supplied pointer passed back unchanged
 */
typedef void (*skity_promise_release_callback)(void* userdata);

/**
 * @brief Create a promise-texture image (v1): @p get_texture receives only
 *        @p userdata. skity invokes @p get_texture when it needs the GPU
 *        texture, and @p release when the promise is no longer needed (image
 *        destroyed).
 * @param format       texture format
 * @param width        pixel width
 * @param height       pixel height
 * @param alpha_type   alpha interpretation
 * @param get_texture  callback returning the current texture
 * @param release      callback invoked when the promise is no longer needed
 * @param userdata     caller-supplied pointer passed back to the callbacks
 * @return             image handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image skity_image_create_promise(
    skity_texture_format format, uint32_t width, uint32_t height,
    skity_alpha_type alpha_type, skity_promise_texture_callback get_texture,
    skity_promise_release_callback release, void* userdata);

/**
 * @brief Create a promise-texture image (v2): identical to
 *        skity_image_create_promise, but @p get_texture also receives the GPU
 *        context the image is being used with.
 * @param format       texture format
 * @param width        pixel width
 * @param height       pixel height
 * @param alpha_type   alpha interpretation
 * @param get_texture  callback returning the current texture for the given
 *                     GPU context
 * @param release      callback invoked when the promise is no longer needed
 * @param userdata     caller-supplied pointer passed back to the callbacks
 * @return             image handle, or NULL on invalid arguments
 */
SKITY_C_API skity_image skity_image_create_promise2(
    skity_texture_format format, uint32_t width, uint32_t height,
    skity_alpha_type alpha_type, skity_promise_texture_callback2 get_texture,
    skity_promise_release_callback release, void* userdata);

/**
 * @brief Read the image's pixels back to CPU memory.
 * @param image    source image
 * @param context  GPU context, required when the image is GPU-backed
 * @return         read-only pixmap handle, or NULL on failure (e.g. the image
 *                 has no readable backing)
 */
SKITY_C_API skity_pixmap skity_image_read_pixels(skity_image image,
                                                 skity_context context);

/**
 * @brief Resample the image's pixels into @p dst, scaling to fill the entire
 *        destination pixmap. @p dst describes the destination buffer (width,
 *        height, row bytes, color / alpha info) and the caller owns its
 *        writable storage. GPU-backed images require a non-NULL @p context to
 *        perform the readback; CPU-backed images accept NULL.
 * @param image    source image
 * @param dst      destination pixmap (writable pixel buffer)
 * @param context  GPU context, required when the image is GPU-backed; may be
 *                 NULL for CPU-backed images
 * @param sampling filter / mipmap / cubic resampling options; NULL uses the
 *                 defaults (nearest-filter, no mipmap, no cubic)
 * @return         1 on success, 0 on failure (NULL image / dst, or the
 *                 backend failed to scale)
 */
SKITY_C_API uint32_t skity_image_scale_pixels(
    skity_image image, skity_pixmap dst, skity_context context,
    const skity_sampling_options* sampling);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_IMAGE_H
