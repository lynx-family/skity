// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_context_mtl.h>

#include <skity/gpu/gpu_context.hpp>

#include "handle.hpp"

#if defined(SKITY_METAL)
#include "mtl_glue.hpp"
#endif

extern "C" {

skity_result skity_context_create_mtl(void* device, void* command_queue,
                                      skity_context* out_context) {
  if (out_context == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
#if defined(SKITY_METAL)
  std::unique_ptr<skity::GPUContext> ctx =
      skity::capi::MtlContextCreateGlue(device, command_queue);
  if (ctx == nullptr) {
    return SKITY_ERROR_INITIALIZATION_FAILED;
  }
  std::shared_ptr<skity::GPUContext> impl(ctx.release());
  skity_context_s* w = skity::capi::alloc_handle<skity_context_s>(
      SKITY_OBJECT_TYPE_CONTEXT, SKITY_HANDLE_OWNING, std::move(impl));
  if (w == nullptr) {
    return SKITY_ERROR_OUT_OF_HOST_MEMORY;
  }
  *out_context = w;
  return SKITY_SUCCESS;
#else
  (void)device;
  (void)command_queue;
  return SKITY_ERROR_NOT_SUPPORTED;
#endif
}

void* skity_context_mtl_get_device(skity_context context) {
#if defined(SKITY_METAL)
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  return ctx ? skity::capi::MtlContextGetDeviceGlue(ctx.get()) : nullptr;
#else
  (void)context;
  return nullptr;
#endif
}

void* skity_context_mtl_get_command_queue(skity_context context) {
#if defined(SKITY_METAL)
  auto ctx = skity::capi::get_impl<skity_context_s, skity::GPUContext>(
      context, SKITY_OBJECT_TYPE_CONTEXT);
  return ctx ? skity::capi::MtlContextGetCommandQueueGlue(ctx.get()) : nullptr;
#else
  (void)context;
  return nullptr;
#endif
}

}  // extern "C"
