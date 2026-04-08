// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <gtest/gtest.h>

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/texture.hpp>
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

  virtual std::shared_ptr<GoldenTexture> DisplayListToTexture(
      DisplayList* dl, uint32_t width, uint32_t height) = 0;

  bool SaveGoldenImage(std::shared_ptr<Pixmap> image, const char* path);

  static GoldenTestEnv* CreateInstance(Backend backend);

  static GoldenTestEnv* GetInstance();

  skity::GPUContext* GetGPUContext() { return gpu_context_.get(); }

 protected:
  virtual std::unique_ptr<skity::GPUContext> CreateGPUContext() = 0;

 private:
  std::unique_ptr<skity::GPUContext> gpu_context_ = nullptr;
};

}  // namespace testing
}  // namespace skity
