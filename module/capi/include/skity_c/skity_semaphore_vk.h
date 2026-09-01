// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SEMAPHORE_VK_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SEMAPHORE_VK_H

#include <skity_c/skity_context.h>
#include <skity_c/skity_surface_vk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an engine-owned Vulkan semaphore for external synchronization.
 * @return new semaphore handle, or NULL on failure
 */
SKITY_C_API skity_semaphore skity_semaphore_create_vk(skity_context context);

/**
 * @brief Import a POSIX sync file descriptor into a Vulkan semaphore. The fd
 *        ownership is transferred to the Vulkan driver on success.
 * @return SKITY_SUCCESS on success, or a SKITY_ERROR_* code on failure
 */
SKITY_C_API skity_result skity_semaphore_import_vk(skity_context context,
                                                   skity_semaphore semaphore,
                                                   int sync_fd);

/** @brief Release a semaphore previously created by
 *         skity_semaphore_create_vk. Safe to call with NULL. */
SKITY_C_API void skity_semaphore_destroy_vk(skity_semaphore semaphore);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SEMAPHORE_VK_H
