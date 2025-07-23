// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_RENDER_PIPELINE_MTL_HPP
#define SRC_GPU_MTL_GPU_RENDER_PIPELINE_MTL_HPP

#import <Metal/Metal.h>

#include <memory>
#include <unordered_map>

#include "src/gpu/gpu_render_pipeline.hpp"

namespace skity {

class GPUDeviceMTL;

class GPURenderPipelineMTL : public GPURenderPipeline {
 public:
  static std::unique_ptr<GPURenderPipelineMTL> Make(
      GPUDeviceMTL& device, const GPURenderPipelineDescriptor& desc);

  GPURenderPipelineMTL(id<MTLRenderPipelineState> mtl_render_pipeline,
                       id<MTLDepthStencilState> mtl_depth_stencil,
                       const GPURenderPipelineDescriptor& desc)
      : GPURenderPipeline(desc),
        mtl_render_pipeline_(mtl_render_pipeline),
        mtl_depth_stencil_(mtl_depth_stencil) {}

  id<MTLRenderPipelineState> GetMTLRenderPipelineState() const {
    return mtl_render_pipeline_;
  }

  id<MTLDepthStencilState> GetMTLDepthStencilState() const {
    return mtl_depth_stencil_;
  }

  bool IsValid() const override {
    return GPURenderPipeline::IsValid() && mtl_render_pipeline_ != nil &&
           mtl_depth_stencil_ != nil;
  }

 private:
  id<MTLRenderPipelineState> mtl_render_pipeline_;
  id<MTLDepthStencilState> mtl_depth_stencil_;
};

struct GPUDepthStencilEqual {
  constexpr bool operator()(const GPUDepthStencilState& lhs,
                            const GPUDepthStencilState& rhs) const {
    return lhs.format == rhs.format &&
           lhs.enable_stencil == rhs.enable_stencil &&
           lhs.stencil_state == rhs.stencil_state &&
           lhs.enable_depth == rhs.enable_depth &&
           lhs.depth_state == rhs.depth_state;
  }
};

struct GPUDepthStencilHash {
  std::size_t operator()(const GPUDepthStencilState& key) const {
    size_t res = 17;
    res = res * 31 + std::hash<uint32_t>()(static_cast<uint32_t>(key.format));
    res = res * 31 +
          std::hash<uint32_t>()(static_cast<uint32_t>(key.enable_stencil));
    res = res * 31 + Hash(key.stencil_state);
    res = res * 31 + Hash(key.depth_state);
    return res;
  }

  std::size_t Hash(const GPUStencilState& state) const {
    size_t res = 17;
    res = res * 31 + Hash(state.front);
    res = res * 31 + Hash(state.back);
    return res;
  }

  std::size_t Hash(const GPUDepthState& state) const {
    size_t res = 17;
    res = res * 31 + std::hash<bool>{}(state.enableWrite);
    res =
        res * 31 + std::hash<uint32_t>{}(static_cast<uint32_t>(state.compare));
    return res;
  }

  std::size_t Hash(const GPUStencilFaceState& face_stage) const {
    size_t res = 17;
    res = res * 31 +
          std::hash<uint32_t>()(static_cast<uint32_t>(face_stage.compare));
    res = res * 31 +
          std::hash<uint32_t>()(static_cast<uint32_t>(face_stage.fail_op));
    res = res * 31 + std::hash<uint32_t>()(
                         static_cast<uint32_t>(face_stage.depth_fail_op));
    res = res * 31 +
          std::hash<uint32_t>()(static_cast<uint32_t>(face_stage.pass_op));
    res = res * 31 + std::hash<uint32_t>()(face_stage.stencil_read_mask);
    res = res * 31 + std::hash<uint32_t>()(face_stage.stencil_write_mask);
    return res;
  }
};

using GPUDepthStencilMap =
    std::unordered_map<GPUDepthStencilState, id<MTLDepthStencilState>,
                       GPUDepthStencilHash, GPUDepthStencilEqual>;

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_RENDER_PIPELINE_MTL_HPP
