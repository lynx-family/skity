// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/gl/window_gl.hpp"

#include <iostream>
#include <skity/gpu/gpu_context_gl.hpp>

namespace skity {
namespace example {

bool WindowGL::OnInit() {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

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

void WindowGL::OnShow() {
  auto window = GetNativeWindow();

  int32_t pp_width, pp_height;
  glfwGetFramebufferSize(window, &pp_width, &pp_height);

  float density = (float)(pp_width * pp_width + pp_height * pp_height) /
                  (float)(GetWidth() * GetWidth() + GetHeight() * GetHeight());
  auto screen_scale = glm::sqrt(density);

  skity::GPUSurfaceDescriptorGL desc{};
  desc.backend = skity::GPUBackendType::kOpenGL;
  desc.width = GetWidth();
  desc.height = GetHeight();
  desc.sample_count = 4;
  desc.content_scale = screen_scale;

  desc.surface_type = skity::GLSurfaceType::kFramebuffer;
  desc.gl_id = 0;
  desc.has_stencil_attachment = true;

  surface_ = GetGPUContext()->CreateSurface(&desc);
}

skity::Canvas* WindowGL::AquireCanvas() {
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
