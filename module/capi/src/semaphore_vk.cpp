// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_semaphore_vk.h>

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_context_vk.hpp>
#include <skity/gpu/gpu_semaphore.hpp>
#include <utility>

#include "handle.hpp"

extern "C" {

skity_semaphore skity_semaphore_create_vk(skity_context context) {
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  if (ctx == nullptr) {
    return nullptr;
  }
  auto semaphore = ctx->CreateSemaphore();
  if (semaphore == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_semaphore_s>(
      SKITY_OBJECT_TYPE_SEMAPHORE, SKITY_HANDLE_OWNING, std::move(semaphore));
}

skity_result skity_semaphore_import_vk(skity_context context,
                                       skity_semaphore semaphore, int sync_fd) {
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  auto sem = skity::capi::get_impl<skity_semaphore_s, skity::GPUSemaphore>(
      semaphore, SKITY_OBJECT_TYPE_SEMAPHORE);
  if (sem == nullptr || ctx == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }
  skity::GPUSemaphoreImportInfoVK info{};
  info.sync_fd = sync_fd;
  ctx->ImportSemaphore(sem.get(), info);
  return SKITY_SUCCESS;
}

void skity_semaphore_destroy_vk(skity_semaphore semaphore) {
  skity::capi::destroy_handle<skity_semaphore_s>(semaphore,
                                                 SKITY_OBJECT_TYPE_SEMAPHORE);
}

}  // extern "C"
