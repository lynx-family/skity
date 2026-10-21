// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_SRC_MTL_GLUE_HPP
#define MODULE_CAPI_SRC_MTL_GLUE_HPP

// Bridge between the portable C wrappers (context_c.cpp, surface_c.cpp,
// texture_c.cpp) and the Objective-C++ Metal backend, plus the typed
// Metal context entry points themselves.
// Declared here in pure C++ and implemented in mtl_glue.mm, which is
// only compiled when SKITY_MTL_BACKEND is on. The glue interface keeps
// plain C++ types only: this header is included from both plain C++ and
// Objective-C++ translation units, and the public Metal extension
// structs are Objective-C-only (skity_mtl_types.h), so mtl_glue.mm
// walks the info->p_next chain and interprets the extension on its side.

#include <skity/c/skity_surface.h>
#include <skity/c/skity_texture.h>

#include <memory>
#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <skity/gpu/texture.hpp>

namespace skity {
namespace capi {

/** Forward to skity::MTLContextCreate with void*-bridged device / queue. */
std::unique_ptr<GPUContext> MtlContextCreateGlue(void* device, void* queue);

/** Forward to skity::MTLContextGetDevice / MTLContextGetCommandQueue. */
void* MtlContextGetDeviceGlue(GPUContext* context);
void* MtlContextGetCommandQueueGlue(GPUContext* context);

/** Read info->p_next as the Metal extension, build a
 *  GPUSurfaceDescriptorMTL and call GPUContext::CreateSurface. */
std::unique_ptr<GPUSurface> MtlSurfaceCreateGlue(
    GPUContext* context, const skity_surface_create_info* info);

/** Read info->p_next as the Metal extension, build a
 *  GPUBackendTextureInfoMTL and call GPUContext::WrapTexture. */
std::shared_ptr<Texture> MtlWrapTextureGlue(
    GPUContext* context, const skity_backend_texture_info* info,
    skity_texture_release_callback release, void* userdata);

}  // namespace capi
}  // namespace skity

#endif  // MODULE_CAPI_SRC_MTL_GLUE_HPP
