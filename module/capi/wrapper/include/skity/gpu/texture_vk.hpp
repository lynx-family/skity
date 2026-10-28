// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_TEXTURE_VK_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_TEXTURE_VK_HPP

#include <skity/c/skity_texture_vk.h>

#include <skity/gpu/texture.hpp>

namespace skity {

/**
 * Wrap an existing VkImage (+ its view) as a Texture on context: chains
 * ext into base's p_next (s_type and the Vulkan backend tag are set
 * automatically) and forwards to Texture::CreateFromBackend. release
 * fires (with userdata) when skity drops its last reference.
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

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_GPU_TEXTURE_VK_HPP
