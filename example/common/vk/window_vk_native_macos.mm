// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include <skity/macros.hpp>

#if defined(SKITY_MACOS)

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include <QuartzCore/CAMetalLayer.h>

namespace skity {
namespace example {

bool SetupCocoaVulkanWindow(GLFWwindow* window) {
  if (window == nullptr) {
    return false;
  }

  NSWindow* cocoa_window = glfwGetCocoaWindow(window);
  if (cocoa_window == nil) {
    return false;
  }

  CAMetalLayer* metal_layer = [CAMetalLayer layer];
  metal_layer.opaque = YES;
  metal_layer.contentsScale = [[NSScreen mainScreen] backingScaleFactor];
  metal_layer.framebufferOnly = NO;
  metal_layer.frame = cocoa_window.contentView.bounds;
  metal_layer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;

  cocoa_window.contentView.layer = metal_layer;
  cocoa_window.contentView.wantsLayer = YES;
  return true;
}

void* GetCocoaVulkanLayer(GLFWwindow* window) {
  if (window == nullptr) {
    return nullptr;
  }

  NSWindow* cocoa_window = glfwGetCocoaWindow(window);
  if (cocoa_window == nil) {
    return nullptr;
  }

  return (__bridge void*)cocoa_window.contentView.layer;
}

void* GetCocoaVulkanView(GLFWwindow* window) {
  if (window == nullptr) {
    return nullptr;
  }

  NSWindow* cocoa_window = glfwGetCocoaWindow(window);
  if (cocoa_window == nil) {
    return nullptr;
  }

  return (__bridge void*)cocoa_window.contentView;
}

}  // namespace example
}  // namespace skity

#endif
