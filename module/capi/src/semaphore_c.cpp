// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_semaphore.h>

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_semaphore.hpp>
#include <utility>

#if defined(SKITY_VULKAN)
#include <skity_c/skity_semaphore_vk.h>

#include <skity/gpu/gpu_context_vk.hpp>
#endif

#include "handle.hpp"

extern "C" {

skity_semaphore skity_semaphore_create(skity_context context) {
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  if (ctx == nullptr) {
    return nullptr;
  }
  // Neutral dispatch on the context's backend; only the Vulkan backend
  // implements semaphores today, the others return nullptr.
  auto semaphore = ctx->CreateSemaphore();
  if (semaphore == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_semaphore_s>(
      SKITY_OBJECT_TYPE_SEMAPHORE, SKITY_HANDLE_OWNING, std::move(semaphore));
}

skity_result skity_semaphore_import(skity_context context,
                                    skity_semaphore semaphore,
                                    const skity_semaphore_import_info* info) {
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  auto sem = skity::capi::get_impl<skity_semaphore_s, skity::GPUSemaphore>(
      semaphore, SKITY_OBJECT_TYPE_SEMAPHORE);
  if (sem == nullptr || ctx == nullptr || info == nullptr) {
    return SKITY_ERROR_INVALID_HANDLE;
  }
  auto st = info->p_next != nullptr
                ? *static_cast<const skity_structure_type*>(info->p_next)
                : SKITY_STRUCTURE_TYPE_SEMAPHORE_IMPORT_INFO;
  const skity::GPUSemaphoreImportInfo* native = nullptr;
#if defined(SKITY_VULKAN)
  skity::GPUSemaphoreImportInfoVK vk_info{};
  if (st == SKITY_STRUCTURE_TYPE_SEMAPHORE_IMPORT_INFO_VK) {
    const auto* ext =
        static_cast<const skity_semaphore_import_info_vk*>(info->p_next);
    vk_info.sync_fd = ext->sync_fd;
    native = &vk_info;
  }
#endif
  if (native == nullptr) {
    return SKITY_ERROR_NOT_SUPPORTED;
  }
  // Backend failures surface through the context error callback; the C++
  // ImportSemaphore itself reports nothing.
  ctx->ImportSemaphore(sem.get(), *native);
  return SKITY_SUCCESS;
}

void skity_semaphore_destroy(skity_semaphore semaphore) {
  skity::capi::destroy_handle<skity_semaphore_s>(semaphore,
                                                 SKITY_OBJECT_TYPE_SEMAPHORE);
}

skity_gpu_backend_type skity_semaphore_get_backend_type(
    skity_semaphore semaphore) {
  auto sem = skity::capi::get_impl<skity_semaphore_s, skity::GPUSemaphore>(
      semaphore, SKITY_OBJECT_TYPE_SEMAPHORE);
  return sem ? static_cast<skity_gpu_backend_type>(sem->GetBackendType())
             : SKITY_GPU_BACKEND_TYPE_NONE;
}

}  // extern "C"
