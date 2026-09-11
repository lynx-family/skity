// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/gl/window_gl.hpp"

#include <cmath>
#include <iostream>
#include <skity/gpu/gpu_context_gl.hpp>

namespace skity {
namespace example {

bool WindowGL::OnInit() {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  if (UseDebugAAPresent()) {
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_FALSE);
  }

  return true;
}

GLFWwindow* WindowGL::CreateWindowHandler() {
  auto window = glfwCreateWindow(GetWidth(), GetHeight(), GetTitle().c_str(),
                                 nullptr, nullptr);

  if (window == nullptr) {
    return nullptr;
  }

  glfwMakeContextCurrent(window);

  return window;
}

std::unique_ptr<skity::GPUContext> WindowGL::CreateGPUContext() {
  return skity::GLContextCreate((void*)glfwGetProcAddress);
}

void WindowGL::OnShow() {}

skity::Canvas* WindowGL::AquireCanvas() {
  auto window = GetNativeWindow();

  int32_t pp_width = 0;
  int32_t pp_height = 0;
  int32_t logical_width = 0;
  int32_t logical_height = 0;
  for (;;) {
    glfwGetFramebufferSize(window, &pp_width, &pp_height);
    glfwGetWindowSize(window, &logical_width, &logical_height);
    if (pp_width > 0 && pp_height > 0 && logical_width > 0 &&
        logical_height > 0) {
      break;
    }
    if (glfwWindowShouldClose(window)) {
      return nullptr;
    }
    glfwWaitEvents();
  }

  const float screen_scale =
      static_cast<float>(std::hypot(pp_width, pp_height) /
                         std::hypot(logical_width, logical_height));
  const float content_scale = UseDebugAAPresent() ? 1.f : screen_scale;

  // The framebuffer scale may change after the window is first displayed.
  if (surface_ == nullptr || surface_->GetWidth() != logical_width ||
      surface_->GetHeight() != logical_height ||
      surface_->ContentScale() != content_scale) {
    skity::GPUSurfaceDescriptorGL desc{};
    desc.backend = skity::GPUBackendType::kOpenGL;
    desc.width = logical_width;
    desc.height = logical_height;
    desc.sample_count = UseDebugAAPresent() ? GetAASampleCount() : 4;
    desc.content_scale = content_scale;
    desc.surface_type = skity::GLSurfaceType::kFramebuffer;
    desc.gl_id = 0;
    desc.has_stencil_attachment = true;

    surface_.reset();
    surface_ = GetGPUContext()->CreateSurface(&desc);
  }

  if (surface_ == nullptr) {
    return nullptr;
  }

  canvas_ = surface_->LockCanvas();
  return canvas_;
}

void WindowGL::OnPresent() {
  canvas_->Flush();

  surface_->Flush();

  canvas_ = nullptr;

  glfwSwapBuffers(GetNativeWindow());
}

void WindowGL::OnTerminate() { surface_.reset(); }

}  // namespace example
}  // namespace skity
