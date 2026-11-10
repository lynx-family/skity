// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_context_gl.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/recorder/display_list.hpp>

#include "common/golden_texture.hpp"

namespace skity {
namespace testing {

enum class Backend {
  kGL,
  kVulkan,
  kMetal,
};

class GoldenTestEnv : public ::testing::Environment {
 public:
  ~GoldenTestEnv() override = default;

  void SetUp() override;

  void TearDown() override;

  virtual Backend GetBackend() const = 0;

  std::shared_ptr<GoldenTexture> DisplayListToTexture(DisplayList* dl,
                                                      uint32_t width,
                                                      uint32_t height) {
    return RenderToTexture(width, height,
                           [dl](Canvas* canvas) { dl->Draw(canvas); });
  }

  std::shared_ptr<GoldenTexture> RenderToTexture(
      uint32_t width, uint32_t height,
      const std::function<void(Canvas*)>& render) {
    return RenderToSurface(width, height, [&](GPUSurface* surface) {
      auto* canvas = surface->LockCanvas();
      render(canvas);
      canvas->Flush();
      surface->Flush();
    });
  }

  virtual std::shared_ptr<GoldenTexture> RenderToSurface(
      uint32_t width, uint32_t height,
      const std::function<void(GPUSurface*)>& render) = 0;

  void SetSampleCount(uint32_t sample_count) { sample_count_ = sample_count; }

  uint32_t GetSampleCount() const { return sample_count_; }

  /**
   * Declares the color space of the surfaces this env creates (the render
   * target space); defaults to sRGB. Color-space golden cases set this
   * before rendering and restore it afterwards.
   */
  void SetTargetColorSpace(ColorSpace color_space) {
    target_color_space_ = color_space;
  }

  const ColorSpace& GetTargetColorSpace() const { return target_color_space_; }

  virtual void SetGLSurfaceMode(std::optional<GLSurfaceMode> mode) {}

  virtual std::optional<GLSurfaceMode> GetGLSurfaceMode() const {
    return std::nullopt;
  }

  virtual void SetGLHasStencilAttachment(
      std::optional<bool> has_stencil_attachment) {}

  virtual std::optional<bool> GetGLHasStencilAttachment() const {
    return std::nullopt;
  }

  bool SaveGoldenImage(std::shared_ptr<Pixmap> image, const char* path);

  static GoldenTestEnv* CreateInstance(Backend backend);

  static GoldenTestEnv* GetInstance();

  skity::GPUContext* GetGPUContext() { return gpu_context_.get(); }

 protected:
  virtual std::unique_ptr<skity::GPUContext> CreateGPUContext() = 0;

 private:
  std::unique_ptr<skity::GPUContext> gpu_context_ = nullptr;
  uint32_t sample_count_ = 4;
  ColorSpace target_color_space_{};
};

}  // namespace testing
}  // namespace skity
