// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <vulkan/vulkan.h>

#include "common/golden_test_env.hpp"

namespace skity {
namespace testing {

class GoldenTestEnvVK : public GoldenTestEnv {
 public:
  GoldenTestEnvVK();
  ~GoldenTestEnvVK() override;

  Backend GetBackend() const override { return Backend::kVulkan; }

  void SetUp() override;
  void TearDown() override;

  std::shared_ptr<GoldenTexture> RenderToTexture(
      uint32_t width, uint32_t height,
      const std::function<void(Canvas*)>& render) override;

 protected:
  std::unique_ptr<skity::GPUContext> CreateGPUContext() override;
};

}  // namespace testing
}  // namespace skity
