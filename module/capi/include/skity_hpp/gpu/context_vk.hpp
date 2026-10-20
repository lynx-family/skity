// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_GPU_CONTEXT_VK_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_GPU_CONTEXT_VK_HPP

#include <skity_c/skity_context_vk.h>

#include <skity_hpp/gpu/context.hpp>

namespace skity {
namespace raii {

/**
 * Create a Vulkan GPUContext that creates and owns its own VkInstance /
 * VkDevice (it cannot be mixed with other Vulkan code). Mirrors the shape
 * of Context::CreateGL: out receives the context on success.
 */
inline skity_result CreateVkContext(
    PFN_vkGetInstanceProcAddr get_instance_proc_addr, Context* out) {
  if (out == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  skity_context handle = nullptr;
  skity_result result =
      skity_context_create_vk(get_instance_proc_addr, &handle);
  if (result == SKITY_SUCCESS) {
    out->reset(handle);
  }
  return result;
}

/**
 * Create a Vulkan GPUContext from caller-provided Vulkan state. Each native
 * handle in info is optional; supplied handles stay owned by (and must
 * outlive) the caller.
 */
inline skity_result CreateVkContext(const skity_context_create_info_vk& info,
                                    Context* out) {
  if (out == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  skity_context handle = nullptr;
  skity_result result = skity_context_create_vk_ex(&info, &handle);
  if (result == SKITY_SUCCESS) {
    out->reset(handle);
  }
  return result;
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_GPU_CONTEXT_VK_HPP
