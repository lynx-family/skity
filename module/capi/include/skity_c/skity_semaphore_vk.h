// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SEMAPHORE_VK_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SEMAPHORE_VK_H

#include <skity_c/skity_semaphore.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Vulkan extension for importing a POSIX sync file descriptor into a
 *        skity_semaphore. Chain it through
 *        skity_semaphore_import_info::p_next with s_type =
 *        SKITY_STRUCTURE_TYPE_SEMAPHORE_IMPORT_INFO_VK.
 *
 * The fd's ownership transfers to the Vulkan driver (consumed even on
 * import failure, per the SYNC_FD external-semaphore spec).
 */
typedef struct skity_semaphore_import_info_vk {
  skity_structure_type s_type;
  const void* p_next;
  int sync_fd; /**< POSIX sync file descriptor */
} skity_semaphore_import_info_vk;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SEMAPHORE_VK_H
