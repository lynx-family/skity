// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SURFACE_VK_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SURFACE_VK_HPP

// VK-only part of the header-only RAII layer: one-shot Vulkan surface
// creation helper chaining the VK CreateInfo extension. Intentionally NOT
// included by skity.hpp — including it pulls in <vulkan/vulkan.h>. See
// skity_vk.hpp for the VK aggregator.

#include <skity_c/skity_surface_vk.h>

#include <skity_hpp/skity_surface.hpp>

namespace skity {
namespace raii {

/**
 * Create a one-shot Vulkan render surface on @p context: chains @p ext into
 * @p base's p_next (s_type set automatically) and forwards to
 * Surface::Create. @p base carries the geometry / sample count; @p ext the
 * VkImage / format / layout / external sync description.
 */
inline skity_result CreateVkSurface(const Context& context,
                                    const skity_surface_create_info& base,
                                    const skity_surface_create_info_vk& ext,
                                    Surface* out) {
  skity_surface_create_info_vk chained = ext;
  chained.s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_VK;
  chained.p_next = nullptr;
  skity_surface_create_info info = base;
  info.s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO;
  info.p_next = &chained;
  return Surface::Create(context, info, out);
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SURFACE_VK_HPP
