// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include <QuartzCore/CAMetalLayer.h>

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
  NSWindow* cocoa_window = window != nullptr ? glfwGetCocoaWindow(window) : nullptr;
  return cocoa_window == nullptr ? nullptr : (__bridge void*)cocoa_window.contentView.layer;
}

void* GetCocoaVulkanView(GLFWwindow* window) {
  NSWindow* cocoa_window = window != nullptr ? glfwGetCocoaWindow(window) : nullptr;
  return cocoa_window == nullptr ? nullptr : (__bridge void*)cocoa_window.contentView;
}
