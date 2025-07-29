// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SKITY_EXAMPLE_COMMON_WINDOW_HPP
#define SKITY_EXAMPLE_COMMON_WINDOW_HPP

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <memory>
#include <skity/gpu/gpu_context.hpp>
#include <string>

namespace skity {
namespace example {

class Window;

class WindowClient {
  friend class Window;

 public:
  virtual ~WindowClient() = default;

  virtual void OnStart(skity::GPUContext* context){};

  virtual void OnDraw(skity::GPUContext* context, skity::Canvas* canvas) = 0;

  virtual void OnTerminate(){};

  const Window* GetWindow() const { return window_; }

 private:
  const Window* window_ = nullptr;
};

class Window {
 public:
  enum class Backend {
    kNone,
    kSoftware,
    kOpenGL,
    kMetal,
    kVulkan,
    kDirectX,
  };

  Window(int width, int height, std::string title)
      : width_(width), height_(height), title_(std::move(title)) {}

  virtual ~Window() = default;

  static std::unique_ptr<Window> CreateWindow(Backend backend, uint32_t width,
                                              uint32_t height,
                                              std::string title);

  void Show(WindowClient& client);

  virtual Backend GetBackend() const = 0;

  void GetCursorPos(double& x, double& y) const;

 protected:
  const std::string& GetTitle() const { return title_; }
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

  GLFWwindow* GetNativeWindow() const { return native_window_; }

  skity::GPUContext* GetGPUContext() const { return gpu_context_.get(); }

  bool Init();

  virtual bool OnInit() = 0;

  virtual GLFWwindow* CreateWindowHandler() = 0;

  virtual std::unique_ptr<skity::GPUContext> CreateGPUContext() = 0;

  virtual void OnShow() = 0;

  virtual skity::Canvas* AquireCanvas() = 0;

  virtual void OnPresent() = 0;

  virtual void OnTerminate() = 0;

 private:
  std::string title_;
  int width_ = 0;
  int height_ = 0;
  GLFWwindow* native_window_ = nullptr;

  std::unique_ptr<skity::GPUContext> gpu_context_;
};

}  // namespace example
}  // namespace skity

#endif  // SKITY_EXAMPLE_COMMON_WINDOW_HPP
