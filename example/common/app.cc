// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/app.hpp"

#include <iostream>

namespace skity {
namespace example {

int StartExampleApp(int argc, const char** argv, WindowClient& client,
                    int width, int height, std::string title) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <backend>" << std::endl;
    // available backends:
    // software
    // gl
    // metal
    // vulkan
    // directx

    std::cerr << "Available backends:" << std::endl;
    std::cerr << "  software" << std::endl;
    std::cerr << "  gl" << std::endl;
    std::cerr << "  metal" << std::endl;
    std::cerr << "  vulkan" << std::endl;
    std::cerr << "  directx" << std::endl;

    return -1;
  }

  std::string backend = argv[1];

  Window::Backend window_backend = Window::Backend::kNone;

  if (backend == "gl") {
    window_backend = Window::Backend::kOpenGL;

    title += " [ GL ] ";
  } else if (backend == "metal") {
    window_backend = Window::Backend::kMetal;

    title += " [ Metal ] ";
  } else if (backend == "vulkan") {
    window_backend = Window::Backend::kVulkan;

    title += " [ Vulkan ] ";
  } else if (backend == "directx") {
    window_backend = Window::Backend::kDirectX;

    title += " [ DirectX ] ";
  } else if (backend == "software") {
    window_backend = Window::Backend::kSoftware;
    title += " [ Software ] ";
  } else {
    std::cerr << "Unknown backend: " << backend << std::endl;
    return -1;
  }

  auto window = Window::CreateWindow(window_backend, width, height, title);

  if (window == nullptr) {
    std::cerr << "Failed to create window." << std::endl;
    return -1;
  }

  window->Show(client);

  return 0;
}

}  // namespace example
}  // namespace skity
