// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_SRC_MTL_GLUE_HPP_
#define MODULE_CAPI_SRC_MTL_GLUE_HPP_

// Bridge between the portable Metal C wrappers (context_mtl_c.cpp,
// surface_c.cpp, texture_c.cpp) and the Objective-C++ Metal backend. The
// glue is declared here in pure C++ and implemented in mtl_glue.mm, which
// is only compiled when SKITY_MTL_BACKEND is on. The glue interface keeps
// plain void* even though the public C headers type their handles as
// Objective-C under __OBJC__: this header is included from both plain
// C++ and Objective-C++ translation units, and id<MTLDevice> / void*
// mangle to different C++ symbols, so a typed interface would not link.

#include <skity_c/skity_surface.h>
#include <skity_c/skity_surface_mtl.h>
#include <skity_c/skity_texture.h>
#include <skity_c/skity_texture_mtl.h>

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

/** Build a GPUSurfaceDescriptorMTL and call GPUContext::CreateSurface. */
std::unique_ptr<GPUSurface> MtlSurfaceCreateGlue(
    GPUContext* context, const skity_surface_create_info* info,
    const skity_surface_create_info_mtl* extension);

/** Build a GPUBackendTextureInfoMTL and call GPUContext::WrapTexture. */
std::shared_ptr<Texture> MtlWrapTextureGlue(
    GPUContext* context, const skity_backend_texture_info* info,
    const skity_backend_texture_info_mtl* extension,
    skity_texture_release_callback release, void* userdata);

}  // namespace capi
}  // namespace skity

#endif  // MODULE_CAPI_SRC_MTL_GLUE_HPP_
