// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/window.hpp"

#include <iostream>
#include <skity/skity.hpp>

#ifdef SKITY_EXAMPLE_GL_BACKEND
#include "common/gl/window_gl.hpp"
#endif

#ifdef SKITY_EXAMPLE_MTL_BACKEND
#include "common/mtl/window_mtl.hpp"
#endif

#ifdef SKITY_EXAMPLE_SW_BACKEND
#include "common/sw/window_sw.hpp"
#endif

#include "common/auto_release_pool.hpp"

namespace skity {
namespace example {

std::unique_ptr<Window> Window::CreateWindow(Backend backend, uint32_t width,
                                             uint32_t height,
                                             std::string title) {
  std::unique_ptr<Window> window;

  if (backend == Backend::kOpenGL) {
#ifdef SKITY_EXAMPLE_GL_BACKEND
    window = std::make_unique<WindowGL>(width, height, std::move(title));
#else
    std::cerr << "OpenGL backend is not supported." << std::endl;
#endif
  } else if (backend == Backend::kMetal) {
#ifdef SKITY_EXAMPLE_MTL_BACKEND
    window = std::make_unique<WindowMTL>(width, height, std::move(title));
#else
    std::cerr << "Metal backend is not supported." << std::endl;
#endif
  } else if (backend == Backend::kSoftware) {
#ifdef SKITY_EXAMPLE_SW_BACKEND
    window = std::make_unique<WindowSW>(width, height, std::move(title));
#else
    std::cerr << "Software backend is not supported." << std::endl;
#endif
  }

  if (window == nullptr) {
    std::cerr << "Failed to create window." << std::endl;
    return std::unique_ptr<Window>();
  }

  if (!window->Init()) {
    std::cerr << "Failed to initialize window." << std::endl;

    return std::unique_ptr<Window>();
  }

  return window;
}

bool Window::Init() {
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW." << std::endl;
    return false;
  }

  if (!OnInit()) {
    std::cerr << "Failed to initialize window." << std::endl;
    return false;
  }

  native_window_ = CreateWindowHandler();

  if (native_window_ == nullptr) {
    std::cerr << "Failed to create native window." << std::endl;
    return false;
  }

  gpu_context_ = CreateGPUContext();

  return true;
}

void Window::Show(WindowClient &client) {
  skity::FontManager::RefDefault()->SetDefaultTypeface(
      skity::Typeface::MakeFromFile(EXAMPLE_DEFAULT_FONT));

  client.window_ = this;

  OnShow();

  client.OnStart(gpu_context_.get());

  while (!glfwWindowShouldClose(native_window_)) {
    AutoReleasePool pool;
    auto canvas = AquireCanvas();

    if (canvas == nullptr) {
      break;
    }

    client.OnDraw(gpu_context_.get(), canvas);

    OnPresent();

    glfwPollEvents();
  }

  client.OnTerminate();

  client.window_ = nullptr;

  OnTerminate();

  glfwTerminate();
}

void Window::GetCursorPos(double &x, double &y) const {
  double mx, my;

  glfwGetCursorPos(native_window_, &mx, &my);

  x = mx;
  y = my;
}

}  // namespace example
}  // namespace skity
