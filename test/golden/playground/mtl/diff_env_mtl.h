// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <Metal/Metal.h>
#include <gtest/gtest.h>

#include <skity/gpu/gpu_context.hpp>

namespace skity {
namespace testing {

class DiffEnvMtlTest : public ::testing::Environment {
 public:
  DiffEnvMtlTest(id<MTLDevice> device, id<MTLCommandQueue> command_queue)
      : device_(device), command_queue_(command_queue) {}

  ~DiffEnvMtlTest() = default;

  static DiffEnvMtlTest* GetInstance();

  void SetUp() override;
  void TearDown() override;

  id<MTLDevice> GetDevice() const { return device_; }
  id<MTLCommandQueue> GetCommandQueue() const { return command_queue_; }

  id<MTLComputePipelineState> GetComputePipelineState() const {
    return diff_pipeline_state_;
  }

  GPUContext* GetGPUContext() const { return gpu_context_.get(); }

 private:
  id<MTLDevice> device_ = nullptr;
  id<MTLCommandQueue> command_queue_ = nullptr;
  id<MTLComputePipelineState> diff_pipeline_state_ = nullptr;

  std::unique_ptr<GPUContext> gpu_context_;
};

}  // namespace testing
}  // namespace skity
