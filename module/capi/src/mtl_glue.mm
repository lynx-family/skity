// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Objective-C++ side of the Metal C API: the typed context entry points
// plus the glue (see mtl_glue.hpp). Only compiled when SKITY_MTL_BACKEND
// is on; without it, context_c.cpp carries NOT_SUPPORTED stubs. The glue
// interface stays plain
// C++; this TU pulls in the Objective-C-only extension headers and
// walks the info->p_next chains here. void* <-> Objective-C pointer
// conversions use __bridge casts so the file compiles both under ARC
// (the darwin framework builds it with Xcode's default) and without it
// (the plain CMake build).

#import <skity/gpu/gpu_context_mtl.h>

#include <skity/c/skity_context_mtl.h>
#include <skity/c/skity_surface_mtl.h>
#include <skity/c/skity_texture_mtl.h>

#include "handle.hpp"
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
                                                 const skity_surface_create_info* info) {
  const auto* extension = static_cast<const skity_surface_create_info_mtl*>(info->p_next);
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
                                            skity_texture_release_callback release,
                                            void* userdata) {
  const auto* extension = static_cast<const skity_backend_texture_info_mtl*>(info->p_next);
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

extern "C" {

skity_result skity_context_create_mtl(skity_mtl_device device,
                                      skity_mtl_command_queue command_queue,
                                      skity_context* out_context) {
  if (out_context == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  std::unique_ptr<skity::GPUContext> ctx =
      skity::capi::MtlContextCreateGlue((__bridge void*)device, (__bridge void*)command_queue);
  if (ctx == nullptr) {
    return SKITY_ERROR_INITIALIZATION_FAILED;
  }
  std::shared_ptr<skity::GPUContext> impl(ctx.release());
  skity_context_s* w = skity::capi::alloc_handle<skity_context_s>(
      SKITY_OBJECT_TYPE_CONTEXT, SKITY_HANDLE_OWNING, std::move(impl));
  if (w == nullptr) {
    return SKITY_ERROR_OUT_OF_HOST_MEMORY;
  }
  *out_context = w;
  return SKITY_SUCCESS;
}

skity_mtl_device skity_context_mtl_get_device(skity_context context) {
  auto ctx =
      skity::capi::get_impl<skity_context_s, skity::GPUContext>(context, SKITY_OBJECT_TYPE_CONTEXT);
  void* device = ctx ? skity::capi::MtlContextGetDeviceGlue(ctx.get()) : nullptr;
  return (__bridge skity_mtl_device)device;
}

skity_mtl_command_queue skity_context_mtl_get_command_queue(skity_context context) {
  auto ctx =
      skity::capi::get_impl<skity_context_s, skity::GPUContext>(context, SKITY_OBJECT_TYPE_CONTEXT);
  void* queue = ctx ? skity::capi::MtlContextGetCommandQueueGlue(ctx.get()) : nullptr;
  return (__bridge skity_mtl_command_queue)queue;
}

}  // extern "C"
