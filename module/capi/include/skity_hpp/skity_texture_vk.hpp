// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXTURE_VK_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXTURE_VK_HPP

// VK-only part of the header-only RAII layer: wrapping an existing VkImage
// as a Texture. Intentionally NOT included by skity.hpp — including it
// pulls in <vulkan/vulkan.h>. See skity_vk.hpp for the VK aggregator.

#include <skity_c/skity_texture_vk.h>

#include <skity_hpp/skity_texture.hpp>

namespace skity {
namespace raii {

/**
 * Wrap an existing VkImage (+ its view) as a Texture on @p context: chains
 * @p ext into @p base's p_next (s_type and the Vulkan backend tag are set
 * automatically) and forwards to Texture::CreateFromBackend. @p release
 * fires (with @p userdata) when skity drops its last reference.
 */
inline Texture WrapVkTexture(const Context& context,
                             const skity_backend_texture_info& base,
                             const skity_backend_texture_info_vk& ext,
                             skity_texture_release_callback release = nullptr,
                             void* userdata = nullptr) {
  skity_backend_texture_info_vk chained = ext;
  chained.s_type = SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_VK;
  chained.p_next = nullptr;
  skity_backend_texture_info info = base;
  info.s_type = SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO;
  info.p_next = &chained;
  info.backend = SKITY_GPU_BACKEND_TYPE_VULKAN;
  return Texture::CreateFromBackend(context, info, release, userdata);
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXTURE_VK_HPP
