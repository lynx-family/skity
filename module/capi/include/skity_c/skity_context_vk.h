// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_VK_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_VK_H

#include <skity_c/skity_base.h>
#include <skity_c/skity_context.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a GPUContext targeting the Vulkan backend.
 *
 * The engine creates and owns its own VkInstance / VkDevice, so the resulting
 * context cannot be mixed with other Vulkan code. The instance and device are
 * not available for outside use.
 *
 * Native Vulkan types (PFN_vkGetInstanceProcAddr) are reused directly because
 * they are already C and ABI-stable.
 *
 * Only available when skity is built with SKITY_VK_BACKEND=ON.
 *
 * @param get_instance_proc_addr  instance procedure-address loader
 * @param out_context             receives the new context handle on success
 * @return SKITY_SUCCESS on success, or a SKITY_ERROR_* code on failure
 */
SKITY_C_API skity_result
skity_context_create_vk(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                        skity_context* out_context);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CONTEXT_VK_H
