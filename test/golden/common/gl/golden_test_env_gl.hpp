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

  std::shared_ptr<GoldenTexture> DisplayListToTexture(DisplayList* dl,
                                                      uint32_t width,
                                                      uint32_t height) override;

 protected:
  std::unique_ptr<skity::GPUContext> CreateGPUContext() override;

 private:
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLSurface surface_ = EGL_NO_SURFACE;
  EGLContext context_ = EGL_NO_CONTEXT;
};

}  // namespace testing
}  // namespace skity
