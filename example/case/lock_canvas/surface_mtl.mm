// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <Metal/Metal.h>

#include <skity/gpu/gpu_context_mtl.h>
#include <functional>

namespace skity::example::lock_canvas {

std::shared_ptr<skity::Image> DrawOffscreenMTL(
    skity::GPUContext *context, int width, int height,
    std::function<void(skity::GPUSurface *surface)> &&func) {
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();

  MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
  desc.width = width;
  desc.height = height;
  desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
  desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;

  id<MTLTexture> texture = [device newTextureWithDescriptor:desc];

  [desc release];

  skity::GPUSurfaceDescriptorMTL desc_mtl{};
  desc_mtl.backend = skity::GPUBackendType::kMetal;
  desc_mtl.width = width;
  desc_mtl.height = height;
  desc_mtl.content_scale = 1;
  desc_mtl.surface_type = skity::MTLSurfaceType::kTexture;
  desc_mtl.texture = texture;

  auto surface = context->CreateSurface(&desc_mtl);

  if (surface == nullptr) {
    return {};
  }

  func(surface.get());
  skity::GPUBackendTextureInfoMTL tex_info{};
  tex_info.backend = skity::GPUBackendType::kMetal;
  tex_info.format = skity::TextureFormat::kRGBA;
  tex_info.width = width;
  tex_info.height = height;
  tex_info.alpha_type = skity::AlphaType::kPremul_AlphaType;
  tex_info.texture = texture;

  auto texture_mtl = context->WrapTexture(&tex_info);

  if (texture_mtl == nullptr) {
    return {};
  }

  return skity::Image::MakeHWImage(texture_mtl);
}

}  // namespace skity::example::lock_canvas
