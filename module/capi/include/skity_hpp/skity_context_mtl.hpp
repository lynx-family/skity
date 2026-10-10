// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_MTL_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_MTL_HPP

// Metal context factories of the header-only RAII layer. The C Metal
// headers type their handles as Objective-C when compiled as Objective-C
// (++) and as void* otherwise (skity_mtl_types.h), so unlike the Vulkan
// wrappers these do NOT need an opt-in aggregate — they are part of the
// neutral skity.hpp umbrella. See skity.hpp for the layer's design.

#include <skity_c/skity_context_mtl.h>

#include <skity_hpp/skity_context.hpp>

namespace skity {
namespace raii {

/**
 * Create a Metal GPUContext. Both @p device (id<MTLDevice>) and
 * @p command_queue (id<MTLCommandQueue>) may be null, letting the engine
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
 * The id<MTLDevice> backing @p context; null for an invalid handle or a
 * non-Metal context. In plain C++ the value is void* — cast it back with
 * `(__bridge id<MTLDevice>)ptr` from Objective-C(++) code.
 */
inline skity_mtl_device GetMtlDevice(const Context& context) {
  return skity_context_mtl_get_device(context.get());
}

/** The id<MTLCommandQueue> backing @p context (see GetMtlDevice). */
inline skity_mtl_command_queue GetMtlCommandQueue(const Context& context) {
  return skity_context_mtl_get_command_queue(context.get());
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_MTL_HPP
