// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_HPP_GPU_SEMAPHORE_VK_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_HPP_GPU_SEMAPHORE_VK_HPP

#include <skity/c/skity_semaphore_vk.h>

#include <skity/hpp/gpu/semaphore.hpp>

namespace skity {

/**
 * Import a POSIX sync file descriptor into semaphore: chains the VK
 * extension into the neutral import info (s_type set automatically). The
 * fd's ownership transfers to the driver — it is consumed even when the
 * import fails, per the SYNC_FD external-semaphore spec.
 */
inline skity_result ImportVkSyncFd(Semaphore& semaphore, const Context& context,
                                   int sync_fd) {
  skity_semaphore_import_info_vk ext = {};
  ext.s_type = SKITY_STRUCTURE_TYPE_SEMAPHORE_IMPORT_INFO_VK;
  ext.sync_fd = sync_fd;
  skity_semaphore_import_info info = {};
  info.s_type = SKITY_STRUCTURE_TYPE_SEMAPHORE_IMPORT_INFO;
  info.p_next = &ext;
  return semaphore.Import(context, info);
}

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_HPP_GPU_SEMAPHORE_VK_HPP
