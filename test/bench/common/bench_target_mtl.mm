// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "test/bench/common/bench_target_mtl.h"

#include <skity/gpu/gpu_context_mtl.h>
#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/gpu/texture.hpp>

#include <cassert>
#include <memory>

namespace skity {

std::shared_ptr<BenchTarget> BenchTargetMTL::Create(skity::GPUContext *context, Options options) {
  skity::GPUSurfaceDescriptorMTL desc;
  desc.backend = skity::GPUBackendType::kMetal;
  desc.width = options.width;
  desc.height = options.height;
  desc.sample_count = options.aa == AAType::kMSAA ? 4 : 1;
  desc.content_scale = 1.f;

  desc.surface_type = skity::MTLSurfaceType::kTexture;
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  MTLTextureDescriptor *texture_desc = [MTLTextureDescriptor new];
  texture_desc.width = options.width;
  texture_desc.height = options.height;
  texture_desc.pixelFormat = MTLPixelFormatBGRA8Unorm;
  texture_desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  texture_desc.textureType = MTLTextureType2D;
  texture_desc.sampleCount = 1;
  texture_desc.storageMode = MTLStorageModePrivate;
  desc.texture = [device newTextureWithDescriptor:texture_desc];
  auto surface = context->CreateSurface(&desc);

  assert(surface.get() != nullptr);
  return std::make_shared<BenchTargetMTL>(context, std::move(surface), options, desc.texture);
}

BenchTargetMTL::BenchTargetMTL(skity::GPUContext *context,
                               std::unique_ptr<skity::GPUSurface> surface, Options options,
                               id<MTLTexture> texture)
    : BenchTarget(context, std::move(surface), options), texture_(texture) {}

}  // namespace skity
