// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Objective-C++ side of the Metal C API glue (see mtl_glue.hpp). Only
// compiled when SKITY_MTL_BACKEND is on. The glue interface passes devices
// and queues as void*, so those conversions use __bridge casts; the create
// / texture-info structs are already typed as Objective-C under __OBJC__.
// __bridge keeps the file compiling both under ARC (the darwin framework
// builds it with Xcode's default) and without it (the plain CMake build).

#import <skity/gpu/gpu_context_mtl.h>

#include "mtl_glue.hpp"

namespace skity {
namespace capi {

std::unique_ptr<GPUContext> MtlContextCreateGlue(void* device, void* queue) {
  return MTLContextCreate((__bridge id<MTLDevice>)device, (__bridge id<MTLCommandQueue>)queue);
}

void* MtlContextGetDeviceGlue(GPUContext* context) {
  return (__bridge void*)MTLContextGetDevice(context);
}

void* MtlContextGetCommandQueueGlue(GPUContext* context) {
  return (__bridge void*)MTLContextGetCommandQueue(context);
}

std::unique_ptr<GPUSurface> MtlSurfaceCreateGlue(GPUContext* context,
                                                 const skity_surface_create_info* info,
                                                 const skity_surface_create_info_mtl* extension) {
  GPUSurfaceDescriptorMTL descriptor{};
  descriptor.backend = GPUBackendType::kMetal;
  descriptor.width = info->width;
  descriptor.height = info->height;
  descriptor.sample_count = info->sample_count ? info->sample_count : 1;
  descriptor.content_scale = info->content_scale != 0.f ? info->content_scale : 1.f;
  switch (extension->surface_type) {
    case SKITY_MTL_SURFACE_TYPE_TEXTURE:
      descriptor.surface_type = MTLSurfaceType::kTexture;
      descriptor.texture = extension->texture;
      break;
    case SKITY_MTL_SURFACE_TYPE_LAYER:
      descriptor.surface_type = MTLSurfaceType::kLayer;
      descriptor.layer = extension->layer;
      break;
    default:
      return nullptr;
  }
  return context->CreateSurface(&descriptor);
}

std::shared_ptr<Texture> MtlWrapTextureGlue(GPUContext* context,
                                            const skity_backend_texture_info* info,
                                            const skity_backend_texture_info_mtl* extension,
                                            skity_texture_release_callback release,
                                            void* userdata) {
  GPUBackendTextureInfoMTL bi{};
  bi.backend = GPUBackendType::kMetal;
  bi.width = info->width;
  bi.height = info->height;
  bi.format = static_cast<TextureFormat>(info->format);
  bi.alpha_type = static_cast<AlphaType>(info->alpha_type);
  bi.texture = extension->texture;
  return context->WrapTexture(&bi, release, userdata);
}

}  // namespace capi
}  // namespace skity
