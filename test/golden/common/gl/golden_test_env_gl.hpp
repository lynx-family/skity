// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <EGL/egl.h>

#include "common/golden_test_env.hpp"

namespace skity {
namespace testing {

class GoldenTestEnvGL : public GoldenTestEnv {
 public:
  GoldenTestEnvGL();

  ~GoldenTestEnvGL() override = default;

  Backend GetBackend() const override { return Backend::kGL; }

  void SetUp() override;

  void TearDown() override;

  std::shared_ptr<GoldenTexture> RenderToTexture(
      uint32_t width, uint32_t height,
      const std::function<void(Canvas*)>& render) override;

  void SetGLSurfaceMode(std::optional<GLSurfaceMode> mode) override {
    surface_mode_ = mode;
  }

  std::optional<GLSurfaceMode> GetGLSurfaceMode() const override {
    return surface_mode_;
  }

  void SetGLHasStencilAttachment(
      std::optional<bool> has_stencil_attachment) override {
    has_stencil_attachment_ = has_stencil_attachment;
  }

  std::optional<bool> GetGLHasStencilAttachment() const override {
    return has_stencil_attachment_;
  }

 protected:
  std::unique_ptr<skity::GPUContext> CreateGPUContext() override;

 private:
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLSurface surface_ = EGL_NO_SURFACE;
  EGLContext context_ = EGL_NO_CONTEXT;
  std::optional<GLSurfaceMode> surface_mode_ = std::nullopt;
  std::optional<bool> has_stencil_attachment_ = std::nullopt;
};

}  // namespace testing
}  // namespace skity
