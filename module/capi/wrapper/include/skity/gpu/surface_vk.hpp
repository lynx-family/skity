// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_SURFACE_VK_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_SURFACE_VK_HPP

#include <skity/c/skity_surface_vk.h>

#include <memory>
#include <skity/gpu/surface.hpp>

namespace skity {

/**
 * Create a one-shot Vulkan render surface on context: chains ext into
 * base's p_next (s_type set automatically) and forwards to
 * Context::CreateSurface. base carries the geometry / sample count; ext the
 * VkImage / format / layout / external sync description.
 */
inline std::unique_ptr<Surface> CreateVkSurface(
    const Context& context, const skity_surface_create_info& base,
    const skity_surface_create_info_vk& ext) {
  skity_surface_create_info_vk chained = ext;
  chained.s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_VK;
  chained.p_next = nullptr;
  skity_surface_create_info info = base;
  info.s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO;
  info.p_next = &chained;
  return context.CreateSurface(info);
}

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_SURFACE_VK_HPP
