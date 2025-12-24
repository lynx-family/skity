// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "src/gpu/mtl/gpu_surface_mtl.h"

#include "src/gpu/mtl/formats_mtl.h"
#include "src/gpu/mtl/gpu_context_impl_mtl.h"
#include "src/render/hw/mtl/mtl_root_layer.h"

namespace skity {

HWRootLayer *GPUSurfaceMTL::OnBeginNextFrame(bool clear) {
  id<MTLTexture> target = AcquireNextTexture();

  auto root_layer = GetArenaAllocator()->Make<MTLRootLayer>(
      static_cast<uint32_t>(GetWidth() * ContentScale()),
      static_cast<uint32_t>(GetHeight() * ContentScale()), Rect::MakeWH(GetWidth(), GetHeight()),
      GetGPUFormat(), target);

  root_layer->SetClearSurface(clear);
  root_layer->SetSampleCount(GetSampleCount());
  root_layer->SetArenaAllocator(GetArenaAllocator());

  return root_layer;
}

MTLTextureSurface::MTLTextureSurface(const GPUSurfaceDescriptor &desc, GPUContextImpl *ctx,
                                     id<MTLTexture> texture)
    : GPUSurfaceMTL(desc, ctx, ToGPUTextureFormat(texture.pixelFormat)), texture_(texture) {}

id<MTLTexture> MTLTextureSurface::AcquireNextTexture() { return texture_; }

MTLLayerSurface::MTLLayerSurface(const GPUSurfaceDescriptor &desc, GPUContextImpl *ctx,
                                 CAMetalLayer *layer)
    : GPUSurfaceMTL(desc, ctx, ToGPUTextureFormat(layer.pixelFormat)),
      layer_(layer),
      queue_(static_cast<GPUContextImplMTL *>(ctx)->GetNativeQueue()) {}

void MTLLayerSurface::OnFlush() {
  if (drawable_ == nil) {
    return;
  }

  id<MTLCommandBuffer> cmd = [queue_ commandBuffer];

  [cmd presentDrawable:drawable_];

  [cmd commit];

  drawable_ = nil;
}

id<MTLTexture> MTLLayerSurface::AcquireNextTexture() {
  drawable_ = [layer_ nextDrawable];

  return [drawable_ texture];
}

}  // namespace skity
