// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXTURE_MTL_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXTURE_MTL_HPP

// Metal texture wrap helper of the header-only RAII layer. Portable like
// its C header (Objective-C pointers as void*), so it lives in the neutral
// skity.hpp umbrella. See skity.hpp for the layer's design.

#include <skity_c/skity_texture_mtl.h>

#include <skity_hpp/skity_texture.hpp>

namespace skity {
namespace raii {

/**
 * Wrap an existing id<MTLTexture> as a Texture on @p context: chains @p ext
 * into @p base's p_next (s_type and the Metal backend tag are set
 * automatically) and forwards to Texture::CreateFromBackend. @p release
 * fires (with @p userdata) when skity drops its last reference.
 */
inline Texture WrapMtlTexture(const Context& context,
                              const skity_backend_texture_info& base,
                              const skity_backend_texture_info_mtl& ext,
                              skity_texture_release_callback release = nullptr,
                              void* userdata = nullptr) {
  skity_backend_texture_info_mtl chained = ext;
  chained.s_type = SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_MTL;
  chained.p_next = nullptr;
  skity_backend_texture_info info = base;
  info.s_type = SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO;
  info.p_next = &chained;
  info.backend = SKITY_GPU_BACKEND_TYPE_METAL;
  return Texture::CreateFromBackend(context, info, release, userdata);
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_TEXTURE_MTL_HPP
