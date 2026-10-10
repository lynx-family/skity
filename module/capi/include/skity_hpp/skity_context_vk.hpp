// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_VK_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_VK_HPP

// VK-only part of the header-only RAII layer: Vulkan context factories.
// This header is intentionally NOT included by skity.hpp — including it
// pulls in <vulkan/vulkan.h>. See skity_vk.hpp for the VK aggregator.

#include <skity_c/skity_context_vk.h>

#include <skity_hpp/skity_context.hpp>

namespace skity {
namespace raii {

/**
 * Create a Vulkan GPUContext that creates and owns its own VkInstance /
 * VkDevice (it cannot be mixed with other Vulkan code). Mirrors the shape
 * of Context::CreateGL: @p out receives the context on success.
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
 * handle in @p info is optional; supplied handles stay owned by (and must
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

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CONTEXT_VK_HPP
