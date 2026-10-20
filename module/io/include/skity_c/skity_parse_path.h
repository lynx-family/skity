// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_INCLUDE_SKITY_C_SKITY_PARSE_PATH_H
#define MODULE_IO_INCLUDE_SKITY_C_SKITY_PARSE_PATH_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_path.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parse an SVG path data string into a new path.
 *
 * Mirrors the legacy ParsePath::FromSVGString; accepts the same command
 * grammar (M/L/H/V/Q/T/S/C/A/Z, relative and absolute).
 *
 * @param data  NUL-terminated SVG path data string
 * @return      a new owning path handle, or NULL when the string does not
 *              parse as a valid path
 */
SKITY_C_API skity_path skity_parse_path_create_from_svg(const char* data);

/**
 * @brief Serialize @p path as an SVG path data string (two-pass idiom).
 *
 * Mirrors the legacy ParsePath::ToSVGString.
 *
 * @param path      path to serialize
 * @param relative  non-zero to emit lower-case relative commands, 0 for
 *                  absolute commands
 * @param out       destination buffer, or NULL to query the length
 * @param out_size  capacity of @p out in bytes
 * @return          string length in bytes including the NUL terminator, or 0
 *                  on an invalid path handle
 */
SKITY_C_API size_t skity_parse_path_to_svg(skity_path path, uint32_t relative,
                                           char* out, size_t out_size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_IO_INCLUDE_SKITY_C_SKITY_PARSE_PATH_H
