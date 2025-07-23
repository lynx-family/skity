// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "playground/window.hpp"

#include <iostream>

#include "common/golden_test_env.hpp"

namespace skity {
namespace testing {

// glfw keyboard callback
static void KeyboardCallback(GLFWwindow* window, int key, int scancode,
                             int action, int mods) {
  if (key == GLFW_KEY_S && action == GLFW_PRESS) {
    auto win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));

    win->SaveGoldenImage();
  }
}

Window::Window(int32_t w, int32_t h, std::string title)
    : width_(w), height_(h), title_(std::move(title)) {}

void Window::Show(bool passed, std::shared_ptr<GoldenTexture> source,
                  std::shared_ptr<Pixmap> target, const char* golden_path) {
  native_window_ = InitWindow();

  if (native_window_ == nullptr) {
    std::cerr << "Failed create native testing window " << std::endl;
    return;
  }

  if (!OnShow(passed, source, target)) {
    std::cerr << "Failed prepare rendering for testing : [" << title_ << "]"
              << std::endl;
    return;
  }

  source_ = source;
  golden_path_ = golden_path;
  glfwSetWindowUserPointer(native_window_, this);
  glfwSetKeyCallback(native_window_, KeyboardCallback);

  while (!glfwWindowShouldClose(native_window_)) {
    OnRender();

    glfwPollEvents();
  }

  OnCloseWindow();

  glfwDestroyWindow(native_window_);
  native_window_ = nullptr;
}

void Window::SaveGoldenImage() {
  if (source_ == nullptr) {
    return;
  }

  auto image = source_->ReadPixels();
  if (image == nullptr) {
    return;
  }

  SaveGoldenImage(image, golden_path_);
}

void Window::SaveGoldenImage(std::shared_ptr<Pixmap> image,
                             const char* golden_path) {
  auto env = GoldenTestEnv::GetInstance();
  if (env == nullptr) {
    return;
  }

  if (!env->SaveGoldenImage(image, golden_path)) {
    std::cerr << "Failed save golden image for test [" << title_ << "]"
              << std::endl;
  }
}

}  // namespace testing
}  // namespace skity
