// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_RENDER_PASS_BUILDER_HPP
#define SRC_RENDER_HW_HW_RENDER_PASS_BUILDER_HPP

#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/hw_draw.hpp"

namespace skity {

struct HWDrawContext;

class HWRenderPassBuilder final {
 public:
  HWRenderPassBuilder(HWDrawContext* ctx, std::shared_ptr<GPUTexture> target)
      : ctx_(ctx), target_(std::move(target)) {}

  ~HWRenderPassBuilder() = default;

  HWRenderPassBuilder& SetSampleCount(uint32_t sample_count) {
    sample_count_ = sample_count;
    return *this;
  }

  HWRenderPassBuilder& SetDrawState(HWDrawState state) {
    state_ = state;

    return *this;
  }

  HWRenderPassBuilder& SetLoadOp(GPULoadOp load_op) {
    load_op_ = load_op;
    return *this;
  }

  HWRenderPassBuilder& SetStoreOp(GPUStoreOp store_op) {
    store_op_ = store_op;
    return *this;
  }

  void Build(GPURenderPassDescriptor& desc);

 private:
  void BuildColorAttachment(GPURenderPassDescriptor& desc);

  void BuildDepthStencilAttachment(GPURenderPassDescriptor& desc);

 private:
  HWDrawContext* ctx_;
  std::shared_ptr<GPUTexture> target_;
  uint32_t sample_count_ = 1;
  HWDrawState state_ = kDrawStateNone;
  GPULoadOp load_op_ = GPULoadOp::kClear;
  GPUStoreOp store_op_ = GPUStoreOp::kStore;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_RENDER_PASS_BUILDER_HPP
