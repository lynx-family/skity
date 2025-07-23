// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <GLFW/glfw3.h>

#include <memory>
#include <skity/skity.hpp>
#include <string>

#include "common/golden_texture.hpp"

namespace skity {
namespace testing {

/**
 * Testing window. Currently there is no GUI.
 * But can use key binding to interact with the testing framework:
 *  key s ----> save the current output as expect golden image
 */
class Window {
 public:
  Window(int32_t w, int32_t h, std::string title);

  virtual ~Window() = default;

  static std::unique_ptr<Window> Create(int32_t w, int32_t h,
                                        std::string title);

  void Show(bool bassed, std::shared_ptr<GoldenTexture> source,
            std::shared_ptr<Pixmap> target, const char* golden_path);

  int32_t GetWidth() const { return width_; }
  int32_t GetHeight() const { return height_; }

  const std::string& GetTitle() const { return title_; }

  void SaveGoldenImage();

 protected:
  virtual GLFWwindow* InitWindow() = 0;

  virtual bool OnShow(bool passed, std::shared_ptr<GoldenTexture> source,
                      std::shared_ptr<Pixmap> target) = 0;

  virtual void OnRender() = 0;

  virtual void OnCloseWindow() = 0;

 private:
  void SaveGoldenImage(std::shared_ptr<Pixmap> image, const char* golden_path);

 private:
  int32_t width_;
  int32_t height_;
  std::string title_;

  GLFWwindow* native_window_ = nullptr;

  std::shared_ptr<GoldenTexture> source_;
  const char* golden_path_ = nullptr;
};

}  // namespace testing
}  // namespace skity
