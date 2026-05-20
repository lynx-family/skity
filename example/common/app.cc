// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/app.hpp"

#include <iostream>

namespace skity {
namespace example {

namespace {

constexpr const char* kAAModePrefix = "--aa=";

const char* AAModeName(Window::AAMode mode) {
  switch (mode) {
    case Window::AAMode::kDefault:
      return "default";
    case Window::AAMode::kNone:
      return "none";
    case Window::AAMode::kContour:
      return "contour";
    case Window::AAMode::kMSAA:
      return "msaa";
  }

  return "unknown";
}

bool ParseAAMode(const std::string& arg, Window::AAMode* aa_mode) {
  if (arg.rfind(kAAModePrefix, 0) != 0) {
    return true;
  }

  auto value = arg.substr(std::string(kAAModePrefix).size());
  if (value == "native") {
    *aa_mode = Window::AAMode::kDefault;
  } else if (value == "none") {
    *aa_mode = Window::AAMode::kNone;
  } else if (value == "contour") {
    *aa_mode = Window::AAMode::kContour;
  } else if (value == "msaa") {
    *aa_mode = Window::AAMode::kMSAA;
  } else {
    std::cerr << "Unknown AA mode: " << value << std::endl;
    std::cerr << "Available AA modes: native, none, contour, msaa" << std::endl;
    return false;
  }

  return true;
}

}  // namespace

int StartExampleApp(int argc, const char** argv, WindowClient& client,
                    int width, int height, std::string title) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <backend> [--aa=<mode>]"
              << std::endl;
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
    std::cerr << "Available AA modes:" << std::endl;
    std::cerr << "  native" << std::endl;
    std::cerr << "  none" << std::endl;
    std::cerr << "  contour" << std::endl;
    std::cerr << "  msaa" << std::endl;

    return -1;
  }

  std::string backend = argv[1];
  Window::AAMode aa_mode = Window::AAMode::kDefault;

  for (int i = 2; i < argc; i++) {
    if (!ParseAAMode(argv[i], &aa_mode)) {
      return -1;
    }
  }

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

  if (aa_mode != Window::AAMode::kDefault) {
    title += " [ AA: ";
    title += AAModeName(aa_mode);
    title += " ] ";
  }

  auto window =
      Window::CreateWindow(window_backend, width, height, title, aa_mode);

  if (window == nullptr) {
    return -1;
  }

  window->Show(client);

  return 0;
}

}  // namespace example
}  // namespace skity
