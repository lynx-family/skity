// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_MTL_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_MTL_HPP

// Metal context factories of the header-only RAII layer. The C Metal
// headers carry Objective-C pointers as void* and stay portable, so unlike
// the Vulkan wrappers these do NOT need an opt-in aggregate — they are
// part of the neutral skity.hpp umbrella. See skity.hpp for the layer's
// design.

#include <skity_c/skity_context_mtl.h>

#include <skity_hpp/skity_context.hpp>

namespace skity {
namespace raii {

/**
 * Create a Metal GPUContext. Both @p device (id<MTLDevice>) and
 * @p command_queue (id<MTLCommandQueue>) may be null, letting the engine
 * pick / create them. Mirrors the shape of Context::CreateGL.
 */
inline skity_result CreateMtlContext(void* device, void* command_queue,
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
 * The id<MTLDevice> backing @p context, as void*; null for an invalid
 * handle or a non-Metal context. Cast back with
 * `(__bridge id<MTLDevice>)ptr` in Objective-C(++) code.
 */
inline void* GetMtlDevice(const Context& context) {
  return skity_context_mtl_get_device(context.get());
}

/** The id<MTLCommandQueue> backing @p context, as void* (see GetMtlDevice). */
inline void* GetMtlCommandQueue(const Context& context) {
  return skity_context_mtl_get_command_queue(context.get());
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_MTL_HPP
