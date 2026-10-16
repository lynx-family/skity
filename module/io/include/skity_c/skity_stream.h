// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_STREAM_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_STREAM_H

#include <skity_c/skity_base.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

SKITY_C_DEFINE_HANDLE(skity_read_stream);
SKITY_C_DEFINE_HANDLE(skity_write_stream);

/**
 * @brief Open a file for reading (skity::ReadStream::CreateFromFile).
 * @param path  file system path
 * @return new stream handle, or NULL when the file cannot be opened
 */
SKITY_C_API skity_read_stream
skity_read_stream_create_from_file(const char* path);

/** @brief Release the read-stream handle. Safe on NULL. */
SKITY_C_API void skity_read_stream_destroy(skity_read_stream stream);

/**
 * @brief Read up to @p size bytes into @p buffer (NULL @p buffer skips
 *        bytes instead).
 * @return number of bytes actually read / skipped
 */
SKITY_C_API size_t skity_read_stream_read(skity_read_stream stream,
                                          void* buffer, size_t size);

/**
 * @brief Peek up to @p size bytes without consuming them.
 * @return number of bytes actually peeked
 */
SKITY_C_API size_t skity_read_stream_peek(skity_read_stream stream,
                                          void* buffer, size_t size);

/** @brief Non-zero when the stream is exhausted. */
SKITY_C_API uint32_t skity_read_stream_is_at_end(skity_read_stream stream);

/** @brief Rewind to the beginning; non-zero on success. */
SKITY_C_API uint32_t skity_read_stream_rewind(skity_read_stream stream);

/**
 * @brief Create a file for writing, truncating an existing one
 *        (skity::WriteStream::CreateFileStream).
 * @param path  file system path
 * @return new stream handle, or NULL when the file cannot be created
 */
SKITY_C_API skity_write_stream
skity_write_stream_create_from_file(const char* path);

/** @brief Release the write-stream handle (flushes pending output). */
SKITY_C_API void skity_write_stream_destroy(skity_write_stream stream);

/**
 * @brief Append @p size bytes from @p buffer.
 * @return non-zero when all bytes were written
 */
SKITY_C_API uint32_t skity_write_stream_write(skity_write_stream stream,
                                              const void* buffer, size_t size);

/** @brief Flush pending output; non-zero on success. */
SKITY_C_API uint32_t skity_write_stream_flush(skity_write_stream stream);

/** @brief Number of bytes written so far. */
SKITY_C_API size_t skity_write_stream_bytes_written(skity_write_stream stream);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_STREAM_H
