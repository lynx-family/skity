// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_CONTEXT_VK_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_CONTEXT_VK_HPP

#include <skity/c/skity_context_vk.h>

#include <memory>
#include <skity/gpu/context.hpp>

namespace skity {

/**
 * Create a Vulkan GPUContext that creates and owns its own VkInstance /
 * VkDevice (it cannot be mixed with other Vulkan code). Mirrors the shape
 * of Context::CreateGL; null on failure.
 */
inline std::unique_ptr<Context> CreateVkContext(
    PFN_vkGetInstanceProcAddr get_instance_proc_addr) {
  skity_context handle = nullptr;
  if (skity_context_create_vk(get_instance_proc_addr, &handle) !=
      SKITY_SUCCESS) {
    return nullptr;
  }
  return detail::ContextCreate::New(handle);
}

/**
 * Create a Vulkan GPUContext from caller-provided Vulkan state. Each native
 * handle in info is optional; supplied handles stay owned by (and must
 * outlive) the caller.
 */
inline std::unique_ptr<Context> CreateVkContext(
    const skity_context_create_info_vk& info) {
  skity_context handle = nullptr;
  if (skity_context_create_vk_ex(&info, &handle) != SKITY_SUCCESS) {
    return nullptr;
  }
  return detail::ContextCreate::New(handle);
}

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_CONTEXT_VK_HPP
