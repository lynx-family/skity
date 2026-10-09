// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SEMAPHORE_VK_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SEMAPHORE_VK_HPP

// VK-only part of the header-only RAII layer: sync-fd semaphore import
// convenience over the neutral Semaphore::Import pass-through. The C
// extension header carries no Vulkan types, but the import itself only
// works on the Vulkan backend. See skity_vk.hpp for the VK aggregator.

#include <skity_c/skity_semaphore_vk.h>

#include <skity_hpp/skity_semaphore.hpp>

namespace skity {
namespace raii {

/**
 * Import a POSIX sync file descriptor into @p semaphore: chains the VK
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

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SEMAPHORE_VK_HPP
