// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "playground/mtl/window_mtl.h"

#include <skity/gpu/gpu_context_mtl.h>
#include <array>
#include <vector>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include "common/mtl/golden_test_env_mtl.h"
#include "common/mtl/golden_texture_mtl.h"

namespace skity {
namespace testing {

constexpr uint kGoldenDiffGroupSize = 16;

std::unique_ptr<Window> Window::Create(int32_t w, int32_t h, std::string title) {
  return std::make_unique<WindowMTL>(w, h, std::move(title));
}

WindowMTL::WindowMTL(int32_t w, int32_t h, std::string title) : Window(w, h, std::move(title)) {}

GLFWwindow* WindowMTL::InitWindow() {
  auto window = glfwCreateWindow(GetWidth(), GetHeight(), GetTitle().c_str(), nullptr, nullptr);

  if (window == nullptr) {
    return nullptr;
  }

  NSWindow* oc_window = glfwGetCocoaWindow(window);

  CAMetalLayer* metal_layer = [CAMetalLayer layer];

  device_ = static_cast<GoldenTestEnvMTL*>(GoldenTestEnv::GetInstance())->GetDevice();
  command_queue_ = static_cast<GoldenTestEnvMTL*>(GoldenTestEnv::GetInstance())->GetCommandQueue();

  metal_layer.device = device_;
  metal_layer.opaque = YES;
  metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  metal_layer.contentsScale = [[NSScreen mainScreen] backingScaleFactor];
  metal_layer.colorspace = CGColorSpaceCreateDeviceRGB();

  oc_window.contentView.layer = metal_layer;
  oc_window.contentView.wantsLayer = YES;

  metal_layer_ = metal_layer;

  gpu_context_ = GoldenTestEnv::GetInstance()->GetGPUContext();

  return window;
}

bool WindowMTL::OnShow(bool passed, std::shared_ptr<GoldenTexture> source,
                       std::shared_ptr<Pixmap> target) {
  if (!InitTextures(passed, source, target)) {
    return false;
  }

  return true;
}

void WindowMTL::OnRender() {
  @autoreleasepool {
    id<CAMetalDrawable> drawable = metal_layer_.nextDrawable;

    if (drawable == nil) {
      return;
    }

    id<MTLTexture> texture = [drawable texture];

    if (texture == nil) {
      return;
    }

    float width = texture.width;
    float height = texture.height;

    skity::GPUSurfaceDescriptorMTL desc{};
    desc.backend = skity::GPUBackendType::kMetal;
    desc.width = width;
    desc.height = height;
    desc.content_scale = 1.0f;
    desc.sample_count = 1;
    desc.surface_type = skity::MTLSurfaceType::kTexture;
    desc.texture = texture;

    auto surface = gpu_context_->CreateSurface(&desc);
    auto canvas = surface->LockCanvas();

    float x = 0.f;
    float y = 0.f;
    // draw source image
    DrawImageInCenter(canvas, source_image_, Rect::MakeXYWH(x, y, width / 2, height / 2));

    x = width / 2;
    // draw target image
    DrawImageInCenter(canvas, target_image_, Rect::MakeXYWH(x, y, width / 2, height / 2));

    // draw diff image
    if (diff_image_ && isolate_diff_image_) {
      x = 0;
      y = height / 2;
      DrawImageInCenter(canvas, diff_image_, Rect::MakeXYWH(x, y, width / 2, height / 2));

      x = width / 2;

      DrawImageInCenter(canvas, isolate_diff_image_, Rect::MakeXYWH(x, y, width / 2, height / 2));
    }

    canvas->Flush();
    surface->Flush();

    id<MTLCommandBuffer> cmd = [command_queue_ commandBuffer];
    [cmd presentDrawable:drawable];
    [cmd commit];
  }
}

void WindowMTL::OnCloseWindow() {
  metal_layer_ = nil;
  device_ = nil;
  command_queue_ = nil;
}

bool WindowMTL::InitTextures(bool passed, std::shared_ptr<GoldenTexture> source,
                             std::shared_ptr<Pixmap> target) {
  source_image_ = source->GetImage();

  if (!source_image_) {
    return false;
  }

  if (!target) {
    auto width = source_image_->Width();
    auto height = source_image_->Height();

    std::vector<uint8_t> data(width * height * 4, 0);

    auto pixel_data = skity::Data::MakeWithCopy(data.data(), data.size());
    target = std::make_shared<skity::Pixmap>(pixel_data, width * 4, width, height,
                                             skity::AlphaType::kPremul_AlphaType,
                                             skity::ColorType::kRGBA);
  }

  target_image_ = skity::Image::MakeImage(target, gpu_context_);

  if (!passed) {
    // create diff image
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.width = source_image_->Width();
    desc.height = source_image_->Height();
    desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    desc.storageMode = MTLStorageModePrivate;

    id<MTLTexture> target_texture = [device_ newTextureWithDescriptor:desc];

    id<MTLTexture> diff_texture = [device_ newTextureWithDescriptor:desc];

    id<MTLTexture> isolate_diff_texture = [device_ newTextureWithDescriptor:desc];

    [desc release];

    {
      // upload target image
      id<MTLBuffer> buffer = [device_ newBufferWithBytes:target->Addr()
                                                  length:target->Width() * target->Height() * 4
                                                 options:MTLResourceStorageModeShared];
      id<MTLCommandBuffer> command_buffer = [command_queue_ commandBuffer];

      id<MTLBlitCommandEncoder> blit_command_encoder = [command_buffer blitCommandEncoder];
      [blit_command_encoder copyFromBuffer:buffer
                              sourceOffset:0
                         sourceBytesPerRow:target->Width() * 4
                       sourceBytesPerImage:target->Width() * target->Height() * 4
                                sourceSize:MTLSizeMake(target->Width(), target->Height(), 1)
                                 toTexture:target_texture
                          destinationSlice:0
                          destinationLevel:0
                         destinationOrigin:MTLOriginMake(0, 0, 0)
                                   options:MTLBlitOptionNone];

      [blit_command_encoder endEncoding];
      [command_buffer commit];
    }

    id<MTLComputePipelineState> pipeline_state =
        static_cast<GoldenTestEnvMTL*>(GoldenTestEnv::GetInstance())->GetComputePipelineState();

    id<MTLCommandBuffer> command_buffer = [command_queue_ commandBuffer];

    id<MTLComputeCommandEncoder> compute_command_encoder = [command_buffer computeCommandEncoder];

    [compute_command_encoder setComputePipelineState:pipeline_state];

    [compute_command_encoder
        setTexture:static_cast<GoldenTextureMTL*>(source.get())->GetMTLTexture()
           atIndex:0];
    [compute_command_encoder setTexture:target_texture atIndex:1];
    [compute_command_encoder setTexture:isolate_diff_texture atIndex:2];
    [compute_command_encoder setTexture:diff_texture atIndex:3];

    auto width = source_image_->Width();
    auto height = source_image_->Height();

    MTLSize threadgroupSize = MTLSizeMake(kGoldenDiffGroupSize, kGoldenDiffGroupSize, 1);
    MTLSize threadgroupsPerGrid =
        MTLSizeMake((width + kGoldenDiffGroupSize - 1) / kGoldenDiffGroupSize,
                    (height + kGoldenDiffGroupSize - 1) / kGoldenDiffGroupSize, 1);

    [compute_command_encoder dispatchThreadgroups:threadgroupsPerGrid
                            threadsPerThreadgroup:threadgroupSize];

    [compute_command_encoder endEncoding];
    [command_buffer commit];

    GPUBackendTextureInfoMTL info{};
    info.backend = GPUBackendType::kMetal;
    info.width = width;
    info.height = height;
    info.format = TextureFormat::kRGBA;
    info.alpha_type = AlphaType::kPremul_AlphaType;

    info.texture = isolate_diff_texture;

    isolate_diff_image_ = Image::MakeHWImage(gpu_context_->WrapTexture(&info));

    info.texture = diff_texture;
    diff_image_ = Image::MakeHWImage(gpu_context_->WrapTexture(&info));
  }

  return true;
}

void WindowMTL::DrawImageInCenter(skity::Canvas* canvas, const std::shared_ptr<Image>& image,
                                  Rect rect) {
  float image_width = image->Width();
  float image_height = image->Height();

  // check if image is too large
  if (image_width > rect.Width() || image_height > rect.Height()) {
    float scale = std::min(rect.Width() / image_width, rect.Height() / image_height);
    image_width *= scale;
    image_height *= scale;
  }

  float x = rect.Left() + (rect.Width() - image_width) / 2.0f;
  float y = rect.Top() + (rect.Height() - image_height) / 2.0f;

  skity::SamplingOptions options{};
  options.filter = skity::FilterMode::kLinear;
  canvas->DrawImage(image, Rect::MakeXYWH(x, y, image_width, image_height), options);
}

}  // namespace testing
}  // namespace skity
