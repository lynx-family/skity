// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/mtl/golden_test_env_mtl.h"
#include <skity/gpu/gpu_context_mtl.h>
#include "common/mtl/golden_texture_mtl.h"

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <iostream>

namespace skity {
namespace testing {

GoldenTestEnv* CreateGoldenTestEnvMTL() { return new GoldenTestEnvMTL(); }

GoldenTestEnvMTL::GoldenTestEnvMTL() {
  device_ = MTLCreateSystemDefaultDevice();
  command_queue_ = [device_ newCommandQueue];
}

std::unique_ptr<skity::GPUContext> GoldenTestEnvMTL::CreateGPUContext() {
  return skity::MTLContextCreate(device_, command_queue_);
}

std::shared_ptr<GoldenTexture> GoldenTestEnvMTL::RenderToTexture(
    uint32_t width, uint32_t height, const std::function<void(Canvas*)>& render) {
  // create a Metal texture
  MTLTextureDescriptor* texture_descriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                         width:width
                                                        height:height
                                                     mipmapped:NO];
  texture_descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  id<MTLTexture> texture = [device_ newTextureWithDescriptor:texture_descriptor];

  if (texture == nil) {
    return {};
  }

  // create a GPUSurface from Metal texture
  skity::GPUSurfaceDescriptorMTL surface_descriptor;
  surface_descriptor.backend = skity::GPUBackendType::kMetal;
  surface_descriptor.width = width;
  surface_descriptor.height = height;
  surface_descriptor.sample_count = 4;
  surface_descriptor.content_scale = 1.f;
  surface_descriptor.texture = texture;
  surface_descriptor.surface_type = skity::MTLSurfaceType::kTexture;

  auto surface = GetGPUContext()->CreateSurface(&surface_descriptor);

  if (surface == nullptr) {
    return {};
  }

  auto canvas = surface->LockCanvas();

  render(canvas);

  canvas->Flush();
  surface->Flush();

  skity::GPUBackendTextureInfoMTL backend_texture_info;
  backend_texture_info.backend = skity::GPUBackendType::kMetal;
  backend_texture_info.width = width;
  backend_texture_info.height = height;

  backend_texture_info.texture = texture;

  auto skity_texture = GetGPUContext()->WrapTexture(&backend_texture_info);

  auto image = skity::Image::MakeHWImage(skity_texture);

  return std::make_shared<GoldenTextureMTL>(std::move(image), texture);
}

bool GoldenTestEnv::SaveGoldenImage(std::shared_ptr<Pixmap> image, const char* path) {
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context =
      CGBitmapContextCreate((void*)image->Addr(), image->Width(), image->Height(), 8,
                            image->Width() * 4, colorSpace, kCGImageAlphaPremultipliedLast);

  if (context == nullptr) {
    CGColorSpaceRelease(colorSpace);
    return false;
  }

  CGImageRef imageRef = CGBitmapContextCreateImage(context);

  CGContextRelease(context);
  CGColorSpaceRelease(colorSpace);

  if (imageRef == nullptr) {
    return false;
  }

  CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8*)path,
                                                         strlen(path), false);

  if (url == nullptr) {
    CFRelease(imageRef);
    return false;
  }

  CFDictionaryRef options = NULL;

  CGImageDestinationRef destination = CGImageDestinationCreateWithURL(url, kUTTypePNG, 1, NULL);

  if (destination == nullptr) {
    CFRelease(url);
    CFRelease(imageRef);
    return false;
  }

  CGImageDestinationAddImage(destination, imageRef, options);

  if (!CGImageDestinationFinalize(destination)) {
    CFRelease(url);
    CFRelease(imageRef);
    CFRelease(destination);
    return false;
  }

  return true;
}

}  // namespace testing
}  // namespace skity
