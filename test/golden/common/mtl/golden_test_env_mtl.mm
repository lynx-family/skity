// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/mtl/golden_test_env_mtl.h"
#include <skity/gpu/gpu_context_mtl.h>
#include "common/mtl/golden_texture_mtl.h"

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#ifdef SKITY_GOLDEN_GUI

#include <GLFW/glfw3.h>

#endif

#include <iostream>

namespace skity {
namespace testing {

const char* diff_shader_source = R"(
  #include <metal_stdlib>
  #include <simd/simd.h>

  using namespace metal;

  kernel void diff_image(texture2d<float, access::read> source_image    [[texture(0)]],
                         texture2d<float, access::read> target_image    [[texture(1)]],
                         texture2d<float, access::write> isolate_image  [[texture(2)]],
                         texture2d<float, access::write> diff_image     [[texture(3)]],
                         uint2 gid                                      [[thread_position_in_grid]])
  {
    if (gid.x >= source_image.get_width() || gid.y >= source_image.get_height()) {
      return;
    }

    float4 source_color = source_image.read(gid);
    float4 target_color = target_image.read(gid);

    if (source_color.a > 0.001) {
      source_color.rgb /= source_color.a;
    }

    float4 diff = abs(source_color - target_color);

    if (diff.x > 0.001 || diff.y > 0.001 || diff.z > 0.001 || diff.w > 0.001) {
      float4 diff_image_color = float4(abs(float3(1.0, 1.0, 1.0) - target_color.rgb), 1.0);

      isolate_image.write(diff_image_color, gid);
      diff_image.write(diff_image_color, gid);
    } else {
      isolate_image.write(float4(0.0, 0.0, 0.0, 0.0), gid);

      diff_image.write(target_color, gid);
    }
  }
)";

void GoldenTestEnvMTL::SetUp() {
  GoldenTestEnv::SetUp();

  NSString* diff_shader_source_string = [NSString stringWithUTF8String:diff_shader_source];

  MTLCompileOptions* compileOptions = [MTLCompileOptions new];
  if (@available(macOS 10.13, iOS 11.0, *)) {
    compileOptions.languageVersion = MTLLanguageVersion2_0;
  }

  NSError* error = nil;

  id<MTLLibrary> library = [device_ newLibraryWithSource:diff_shader_source_string
                                                 options:compileOptions
                                                   error:&error];

  if (error != nil) {
    std::cerr << "diff shader compile error: " << error.localizedDescription.UTF8String
              << std::endl;
  }

  if (library == nil) {
    return;
  }

  diff_pipeline_state_ =
      [device_ newComputePipelineStateWithFunction:[library newFunctionWithName:@"diff_image"]
                                             error:&error];
  if (error != nil) {
    std::cerr << "diff pipeline state create error: " << error.localizedDescription.UTF8String
              << std::endl;
  }

  if (diff_pipeline_state_ == nil) {
    std::cerr << "Failed create diff pipeline" << std::endl;
  }

#ifdef SKITY_GOLDEN_GUI
  // Init glfw for all testing case
  glfwInit();
  // Metal does not needs GL client api
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif
}

void GoldenTestEnvMTL::TearDown() {
  GoldenTestEnv::TearDown();
  diff_pipeline_state_ = nil;

#ifdef SKITY_GOLDEN_GUI

  glfwTerminate();

#endif
}

GoldenTestEnv* CreateGoldenTestEnvMTL() { return new GoldenTestEnvMTL(); }

GoldenTestEnvMTL::GoldenTestEnvMTL() {
  device_ = MTLCreateSystemDefaultDevice();
  command_queue_ = [device_ newCommandQueue];
}

std::unique_ptr<skity::GPUContext> GoldenTestEnvMTL::CreateGPUContext() {
  return skity::MTLContextCreate(device_, command_queue_);
}

std::shared_ptr<GoldenTexture> GoldenTestEnvMTL::DisplayListToTexture(DisplayList* dl,
                                                                      uint32_t width,
                                                                      uint32_t height) {
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

  dl->Draw(canvas);

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

bool GoldenTestEnvMTL::SaveGoldenImage(std::shared_ptr<Pixmap> image, const char* path) {
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
