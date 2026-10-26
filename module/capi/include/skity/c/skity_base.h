// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_BASE_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_BASE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Symbol visibility. The wrapper library (libskity-capi.so) is built with
 * -fvisibility=hidden, so every exported function must carry this attribute.
 * This mirrors the SKITY_API macro used by the C++ headers, but is defined here
 * independently so that the C headers do not depend on any C++ header.
 */
#if defined(_WIN32) || defined(_WIN64)
#define SKITY_C_API __declspec(dllexport)
#elif defined(_MSC_VER)
#define SKITY_C_API __declspec(dllexport)
#else
#define SKITY_C_API __attribute__((visibility("default")))
#endif

/**
 * Defines an opaque handle type backed by a wrapper struct whose first member
 * is skity_object_header. The struct definition itself lives inside the
 * wrapper implementation and is never visible to C consumers.
 *
 *   SKITY_C_DEFINE_HANDLE(skity_paint);
 *   // expands to: typedef struct skity_paint_s* skity_paint;
 */
#define SKITY_C_DEFINE_HANDLE(name) typedef struct name##_s* name

/**
 * NOTE: the handle header (type tag + ownership flags) and the object-type
 * enum are implementation details of the wrapper (see src/handle.hpp) and are
 * intentionally NOT part of the public C ABI. Handles are fully opaque to
 * consumers — they only ever hold and pass the pointer around.
 */

/**
 * @brief Result codes returned by all creating / querying entry points.
 *        Negative values indicate failure.
 */
typedef enum {
  SKITY_SUCCESS = 0,               /**< operation succeeded */
  SKITY_ERROR_INVALID_HANDLE = -1, /**< the passed handle is NULL or invalid */
  SKITY_ERROR_INVALID_ARGUMENT = -2, /**< one or more arguments are invalid */
  SKITY_ERROR_INITIALIZATION_FAILED =
      -3, /**< an internal object could not be initialized */
  SKITY_ERROR_OUT_OF_HOST_MEMORY = -4, /**< host memory allocation failed */
  SKITY_ERROR_NOT_SUPPORTED =
      -5, /**< the operation is not supported on this build/backend */
  SKITY_ERROR_NEED_RECREATE = -6, /**< a swapchain/resize retry is required */
} skity_result;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_BASE_H
