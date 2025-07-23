// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/gpu/gpu_context_mtl.h>

#include <Metal/Metal.h>

namespace skity::example::image {

std::shared_ptr<skity::Image> MakeImageMTL(const std::shared_ptr<skity::Pixmap> &pixmap,
                                           skity::GPUContext *gpu_context) {
  if (!pixmap) {
    return {};
  }

  id<MTLDevice> device = MTLCreateSystemDefaultDevice();

  MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
  desc.width = pixmap->Width();
  desc.height = pixmap->Height();
  desc.pixelFormat = MTLPixelFormatRGBA8Unorm;

  id<MTLTexture> mtl_texture = [device newTextureWithDescriptor:desc];
  [mtl_texture replaceRegion:MTLRegionMake2D(0, 0, desc.width, desc.height)
                 mipmapLevel:0
                   withBytes:pixmap->Addr()
                 bytesPerRow:desc.width * 4];

  skity::GPUBackendTextureInfoMTL info{};
  info.backend = skity::GPUBackendType::kMetal;
  info.width = pixmap->Width();
  info.height = pixmap->Height();
  info.format = skity::TextureFormat::kRGBA;
  info.alpha_type = pixmap->GetAlphaType();
  info.texture = mtl_texture;

  auto texture = gpu_context->WrapTexture(&info);

  [device release];

  return skity::Image::MakeHWImage(texture);
}

}  // namespace skity::example::image
