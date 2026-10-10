// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_GPU_CONTEXT_MTL_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_GPU_CONTEXT_MTL_HPP

#include <skity_c/skity_context_mtl.h>

#include <skity_hpp/gpu/context.hpp>

namespace skity {
namespace raii {

/**
 * Create a Metal GPUContext. Both device (id<MTLDevice>) and
 * command_queue (id<MTLCommandQueue>) may be null, letting the engine
 * pick / create them. Mirrors the shape of Context::CreateGL. The handles
 * are the real Objective-C types in Objective-C(++) code and void*
 * elsewhere.
 */
inline skity_result CreateMtlContext(skity_mtl_device device,
                                     skity_mtl_command_queue command_queue,
                                     Context* out) {
  if (out == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  skity_context handle = nullptr;
  skity_result result =
      skity_context_create_mtl(device, command_queue, &handle);
  if (result == SKITY_SUCCESS) {
    out->reset(handle);
  }
  return result;
}

/**
 * The id<MTLDevice> backing context; null for an invalid handle or a
 * non-Metal context. In plain C++ the value is void* — cast it back with
 * `(__bridge id<MTLDevice>)ptr` from Objective-C(++) code.
 */
inline skity_mtl_device GetMtlDevice(const Context& context) {
  return skity_context_mtl_get_device(context.get());
}

/** The id<MTLCommandQueue> backing context (see GetMtlDevice). */
inline skity_mtl_command_queue GetMtlCommandQueue(const Context& context) {
  return skity_context_mtl_get_command_queue(context.get());
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_GPU_CONTEXT_MTL_HPP
