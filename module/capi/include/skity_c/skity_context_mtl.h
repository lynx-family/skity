// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_MTL_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_MTL_H

// Metal backend entry points. Unlike the Vulkan headers, this header stays
// pure C and portable: Objective-C object pointers (id<MTLDevice>,
// id<MTLCommandQueue>, CAMetalLayer*, id<MTLTexture>) cross the ABI as
// void*, so non-Apple platforms can still compile (and get
// SKITY_ERROR_NOT_SUPPORTED at runtime when skity is built without
// SKITY_MTL_BACKEND).

#include <skity_c/skity_base.h>
#include <skity_c/skity_context.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a GPUContext targeting the Metal backend.
 *
 * Only available when skity is built with SKITY_MTL_BACKEND=ON.
 *
 * @param device  existing id<MTLDevice>; NULL lets the engine pick the
 *                default device
 * @param command_queue  existing id<MTLCommandQueue>; NULL lets the engine
 *                create its own queue on @p device
 * @param out_context  receives the new context handle on success
 * @return SKITY_SUCCESS on success, SKITY_ERROR_NOT_SUPPORTED when built
 *         without the Metal backend, or another SKITY_ERROR_* on failure
 */
SKITY_C_API skity_result skity_context_create_mtl(void* device,
                                                  void* command_queue,
                                                  skity_context* out_context);

/**
 * @brief Return the id<MTLDevice> backing this context, as void*.
 *         NULL for an invalid handle or a non-Metal context.
 */
SKITY_C_API void* skity_context_mtl_get_device(skity_context context);

/**
 * @brief Return the id<MTLCommandQueue> backing this context, as void*.
 *         NULL for an invalid handle or a non-Metal context.
 */
SKITY_C_API void* skity_context_mtl_get_command_queue(skity_context context);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_MTL_H
