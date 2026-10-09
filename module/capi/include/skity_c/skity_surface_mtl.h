// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SURFACE_MTL_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SURFACE_MTL_H

// Metal backend extension for surface creation. Pure C / portable: the
// Objective-C pointers cross the ABI as void* (see skity_context_mtl.h).

#include <skity_c/skity_base.h>
#include <skity_c/skity_surface.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Metal render target kind. */
typedef enum {
  SKITY_MTL_SURFACE_TYPE_INVALID = 0,
  /** target a one-shot id<MTLTexture> (must be created with
   *  MTLTextureUsageRenderTarget) */
  SKITY_MTL_SURFACE_TYPE_TEXTURE,
  /** target a CAMetalLayer (the caller manages layer sizing) */
  SKITY_MTL_SURFACE_TYPE_LAYER,
} skity_mtl_surface_type;

/**
 * @brief Metal backend extension for surface creation. Chain it through
 *        skity_surface_create_info::p_next with s_type =
 *        SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_MTL.
 */
typedef struct skity_surface_create_info_mtl {
  skity_structure_type s_type;
  const void* p_next;
  skity_mtl_surface_type surface_type;
  void* layer;   /**< CAMetalLayer* (SKITY_MTL_SURFACE_TYPE_LAYER) */
  void* texture; /**< id<MTLTexture> (SKITY_MTL_SURFACE_TYPE_TEXTURE) */
} skity_surface_create_info_mtl;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_SURFACE_MTL_H
