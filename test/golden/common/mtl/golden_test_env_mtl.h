// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <Metal/Metal.h>

#include "common/golden_test_env.hpp"

namespace skity {
namespace testing {

class GoldenTestEnvMTL : public GoldenTestEnv {
 public:
  GoldenTestEnvMTL();

  ~GoldenTestEnvMTL() override = default;

  void SetUp() override;

  void TearDown() override;

  std::shared_ptr<GoldenTexture> DisplayListToTexture(
      std::unique_ptr<DisplayList> dl, uint32_t width,
      uint32_t height) override;

  bool SaveGoldenImage(std::shared_ptr<Pixmap> image,
                       const char* path) override;

  id<MTLDevice> GetDevice() const { return device_; }

  id<MTLCommandQueue> GetCommandQueue() const { return command_queue_; }

  id<MTLComputePipelineState> GetComputePipelineState() const {
    return diff_pipeline_state_;
  }

 protected:
  std::unique_ptr<skity::GPUContext> CreateGPUContext() override;

 private:
  id<MTLDevice> device_ = nullptr;
  id<MTLCommandQueue> command_queue_ = nullptr;
  id<MTLComputePipelineState> diff_pipeline_state_ = nullptr;
};

}  // namespace testing
}  // namespace skity
