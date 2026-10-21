// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_MTL_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_MTL_H

// Metal backend entry points. Unlike the Vulkan headers this header is
// Objective-C-only (see skity_mtl_types.h): the handles are the real
// Objective-C types and there is no plain-C / C++ void* fallback, so it
// is only included from Objective-C(++) translation units — like the
// Vulkan headers it is not part of the skity.h umbrella. Builds without
// SKITY_MTL_BACKEND
// still link: the entry points return SKITY_ERROR_NOT_SUPPORTED at
// runtime (portable stubs in context_c.cpp).

#include <skity/c/skity_base.h>
#include <skity/c/skity_context.h>
#include <skity/c/skity_mtl_types.h>

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
SKITY_C_API skity_result skity_context_create_mtl(
    skity_mtl_device device, skity_mtl_command_queue command_queue,
    skity_context* out_context);

/**
 * @brief Return the id<MTLDevice> backing this context.
 *         NULL for an invalid handle or a non-Metal context.
 */
SKITY_C_API skity_mtl_device
skity_context_mtl_get_device(skity_context context);

/**
 * @brief Return the id<MTLCommandQueue> backing this context.
 *         NULL for an invalid handle or a non-Metal context.
 */
SKITY_C_API skity_mtl_command_queue
skity_context_mtl_get_command_queue(skity_context context);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_MTL_H
