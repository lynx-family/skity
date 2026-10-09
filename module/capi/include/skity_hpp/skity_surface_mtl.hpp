// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SURFACE_MTL_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SURFACE_MTL_HPP

// Metal one-shot surface creation helper of the header-only RAII layer.
// Portable like its C header (Objective-C pointers as void*), so it lives
// in the neutral skity.hpp umbrella. See skity.hpp for the layer's design.

#include <skity_c/skity_surface_mtl.h>

#include <skity_hpp/skity_surface.hpp>

namespace skity {
namespace raii {

/**
 * Create a Metal render surface on @p context: chains @p ext into @p base's
 * p_next (s_type set automatically) and forwards to Surface::Create.
 * @p base carries the geometry / sample count; @p ext the target (an
 * id<MTLTexture> with MTLTextureUsageRenderTarget, or a CAMetalLayer the
 * caller keeps sizing).
 */
inline skity_result CreateMtlSurface(const Context& context,
                                     const skity_surface_create_info& base,
                                     const skity_surface_create_info_mtl& ext,
                                     Surface* out) {
  skity_surface_create_info_mtl chained = ext;
  chained.s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_MTL;
  chained.p_next = nullptr;
  skity_surface_create_info info = base;
  info.s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO;
  info.p_next = &chained;
  return Surface::Create(context, info, out);
}

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SURFACE_MTL_HPP
