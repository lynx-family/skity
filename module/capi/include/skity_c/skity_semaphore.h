// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SEMAPHORE_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SEMAPHORE_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_context.h>
#include <skity_c/skity_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a skity::GPUSemaphore.
 *
 * A backend-neutral GPU synchronization semaphore, mirroring the C++
 * GPUContext::CreateSemaphore / ImportSemaphore pair: create it on a context,
 * optionally re-import a native sync handle (backend-specific extension
 * chained through skity_semaphore_import_info::p_next), then pass it to
 * skity_surface_add_external_wait_semaphore before flushing. The
 * semaphore is reusable across frames — each import re-binds a new handle.
 *
 * Note: only the Vulkan backend currently implements semaphores; on other
 * backends skity_semaphore_create returns NULL.
 */
SKITY_C_DEFINE_HANDLE(skity_semaphore);

/**
 * @brief Base descriptor for importing a native sync handle into a
 *        semaphore. Chain a backend-specific extension through @p p_next
 *        (e.g. skity_semaphore_import_info_vk from skity_semaphore_vk.h).
 */
typedef struct skity_semaphore_import_info {
  skity_structure_type s_type; /**< SKITY_STRUCTURE_TYPE_SEMAPHORE_IMPORT_INFO
                                */
  const void* p_next;          /**< → skity_semaphore_import_info_vk, … */
} skity_semaphore_import_info;

/**
 * @brief Create an engine-owned semaphore on @p context's backend.
 *
 * Mirrors GPUContext::CreateSemaphore; currently only the Vulkan backend
 * implements it, other backends return NULL.
 *
 * @return a new owning semaphore handle, or NULL on failure
 */
SKITY_C_API skity_semaphore skity_semaphore_create(skity_context context);

/**
 * @brief Import a native sync handle into @p semaphore.
 *
 * Mirrors GPUContext::ImportSemaphore. The handle's ownership transfers to
 * the GPU driver (consumed even on import failure, per the Vulkan SYNC_FD
 * spec). Backend failures surface through the context's error callback —
 * this call itself only reports handle / argument problems.
 *
 * @param info  base descriptor whose p_next chains the backend-specific
 *              extension; currently
 * SKITY_STRUCTURE_TYPE_SEMAPHORE_IMPORT_INFO_VK
 * @return SKITY_SUCCESS, or SKITY_ERROR_INVALID_HANDLE / NOT_SUPPORTED
 */
SKITY_C_API skity_result
skity_semaphore_import(skity_context context, skity_semaphore semaphore,
                       const skity_semaphore_import_info* info);

/** @brief Release the semaphore handle and its underlying object. Safe on
 *         NULL. */
SKITY_C_API void skity_semaphore_destroy(skity_semaphore semaphore);

/** @brief Return the backend the semaphore was created on
 *         (SKITY_GPU_BACKEND_TYPE_NONE on an invalid handle). */
SKITY_C_API skity_gpu_backend_type
skity_semaphore_get_backend_type(skity_semaphore semaphore);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SEMAPHORE_H
