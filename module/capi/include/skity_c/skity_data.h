// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_DATA_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_DATA_H

#include <skity_c/skity_base.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Immutable byte buffer, mirroring skity::Data. The pointer returned
 *        by skity_data_get_data stays valid for the life of the handle and is
 *        always the same address.
 */
SKITY_C_DEFINE_HANDLE(skity_data);

/**
 * @brief Buffer release callback invoked when the last reference to a wrapped
 *        buffer goes away. ABI-compatible with skity::Data::ReleaseProc.
 *
 * @param ptr     the buffer pointer originally wrapped
 * @param context the opaque pointer passed at creation time
 */
typedef void (*skity_data_release_proc)(const void* ptr, void* context);

/**
 * @brief Create a new buffer holding a copy of the first @p length bytes of
 *        @p data.
 * @param data   source bytes to copy (may be NULL when @p length is 0)
 * @param length number of bytes to copy
 * @return       a new handle, or NULL on allocation failure
 */
SKITY_C_API skity_data skity_data_make_with_copy(const void* data,
                                                 size_t length);

/**
 * @brief Wrap an existing buffer without copying it; @p proc runs with
 *        @p context once the last reference to the data is dropped.
 *
 * This is how foreign memory (locked AndroidBitmap pixels, a CG-owned raster,
 * a malloc'ed plane) enters skity: the caller keeps writing through the
 * original pointer while skity holds a reference, and the release callback is
 * the place to unlock/free the source.
 *
 * @param ptr     buffer to wrap; must stay valid until @p proc runs
 * @param length  size of the buffer in bytes
 * @param proc    release callback, or NULL for no notification
 * @param context opaque pointer passed back to @p proc
 * @return        a new handle, or NULL on allocation failure
 */
SKITY_C_API skity_data skity_data_make_with_proc(const void* ptr, size_t length,
                                                 skity_data_release_proc proc,
                                                 void* context);

/**
 * @brief Load a whole file into memory.
 * @param path  filesystem path of the file to load
 * @return      a new handle, or NULL if the file could not be opened or read
 */
SKITY_C_API skity_data skity_data_make_from_file(const char* path);

/** @brief Return a shared, always-non-NULL empty buffer (size() == 0). */
SKITY_C_API skity_data skity_data_make_empty(void);

/** @brief Release the data handle and its underlying buffer. Safe on NULL. */
SKITY_C_API void skity_data_destroy(skity_data data);

/** @brief Return the number of bytes stored in the buffer. */
SKITY_C_API size_t skity_data_get_size(skity_data data);

/**
 * @brief Return a read-only pointer to the bytes. The address is stable for
 *        the life of the handle; returns NULL for an empty buffer.
 * @return pointer to the immutable bytes
 */
SKITY_C_API const void* skity_data_get_data(skity_data data);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_DATA_H
