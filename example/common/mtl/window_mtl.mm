// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "common/mtl/window_mtl.hpp"

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include <skity/gpu/gpu_context_mtl.h>

namespace skity {
namespace example {

bool WindowMTL::OnInit() {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  return true;
}

GLFWwindow* WindowMTL::CreateWindowHandler() {
  auto window = glfwCreateWindow(GetWidth(), GetHeight(), GetTitle().c_str(), nullptr, nullptr);

  if (window == nullptr) {
    return nullptr;
  }

  NSWindow* oc_window = glfwGetCocoaWindow(window);

  CAMetalLayer* metal_layer = [CAMetalLayer layer];

  metal_layer.device = MTLCreateSystemDefaultDevice();
  metal_layer.opaque = YES;
  metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  metal_layer.contentsScale = [[NSScreen mainScreen] backingScaleFactor];
  metal_layer.colorspace = CGColorSpaceCreateDeviceRGB();

  oc_window.contentView.layer = metal_layer;
  oc_window.contentView.wantsLayer = YES;

  return window;
}

std::unique_ptr<skity::GPUContext> WindowMTL::CreateGPUContext() {
  NSWindow* oc_window = glfwGetCocoaWindow(GetNativeWindow());

  CAMetalLayer* metal_layer = (CAMetalLayer*)oc_window.contentView.layer;

  id<MTLDevice> device = metal_layer.device;

  return skity::MTLContextCreate(device, [device newCommandQueue]);
}

void WindowMTL::OnShow() {
  NSWindow* oc_window = glfwGetCocoaWindow(GetNativeWindow());

  CAMetalLayer* metal_layer = (CAMetalLayer*)oc_window.contentView.layer;

  skity::GPUSurfaceDescriptorMTL desc{};
  desc.backend = skity::GPUBackendType::kMetal;
  desc.width = metal_layer.frame.size.width;
  desc.height = metal_layer.frame.size.height;
  desc.content_scale = metal_layer.contentsScale;
  desc.sample_count = 4;
  desc.surface_type = skity::MTLSurfaceType::kLayer;
  desc.layer = metal_layer;

  surface_ = GetGPUContext()->CreateSurface(&desc);
}

skity::Canvas* WindowMTL::AquireCanvas() {
  if (surface_ == nullptr) {
    return nullptr;
  }

  canvas_ = surface_->LockCanvas();

  return canvas_;
}

void WindowMTL::OnPresent() {
  canvas_->Flush();

  surface_->Flush();

  canvas_ = nullptr;
}

void WindowMTL::OnTerminate() { surface_ = nullptr; }

}  // namespace example
}  // namespace skity
