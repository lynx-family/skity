// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXTURE_MTL_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXTURE_MTL_H

// Metal backend extension for wrapping an existing Metal texture. Pure C /
// portable: the Objective-C pointer crosses the ABI as void* (see
// skity_context_mtl.h).

#include <skity_c/skity_texture.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Metal backend extension for wrapping an existing id<MTLTexture>.
 *        Chain it through skity_backend_texture_info::p_next with s_type =
 *        SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_MTL.
 */
typedef struct skity_backend_texture_info_mtl {
  skity_structure_type s_type;
  const void* p_next;
  void* texture; /**< id<MTLTexture> to wrap */
} skity_backend_texture_info_mtl;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_TEXTURE_MTL_H
