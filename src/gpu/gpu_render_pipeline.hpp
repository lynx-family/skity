// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_RENDER_PIPELINE_HPP
#define SRC_GPU_GPU_RENDER_PIPELINE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "src/gpu/gpu_shader_function.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

enum class GPUCompareFunction {
  kNever,
  kLess,
  kEqual,
  kLessEqual,
  kGreater,
  kNotEqual,
  kGreaterEqual,
  kAlways,
};

enum class GPUStencilOperation {
  kKeep,
  kZero,
  kReplace,
  kInvert,
  kIncrementClamp,
  kDecrementClamp,
  kIncrementWrap,
  kDecrementWrap,
};

struct GPUStencilFaceState {
  GPUCompareFunction compare = GPUCompareFunction::kAlways;
  GPUStencilOperation fail_op = GPUStencilOperation::kKeep;
  GPUStencilOperation depth_fail_op = GPUStencilOperation::kKeep;
  GPUStencilOperation pass_op = GPUStencilOperation::kKeep;
  uint32_t stencil_read_mask = 0xFFFFFFFF;
  uint32_t stencil_write_mask = 0xFFFFFFFF;

  bool operator==(const GPUStencilFaceState& other) const {
    return compare == other.compare && fail_op == other.fail_op &&
           depth_fail_op == other.depth_fail_op && pass_op == other.pass_op &&
           stencil_read_mask == other.stencil_read_mask &&
           stencil_write_mask == other.stencil_write_mask;
  }
};

struct GPUStencilState {
  GPUStencilFaceState front;
  GPUStencilFaceState back;

  bool operator==(const GPUStencilState& other) const {
    return front == other.front && back == other.back;
  }
};

struct GPUDepthState {
  bool enableWrite = true;
  GPUCompareFunction compare = GPUCompareFunction::kAlways;

  bool operator==(const GPUDepthState& other) const {
    return enableWrite == other.enableWrite && compare == other.compare;
  }
};

struct GPUDepthStencilState {
  GPUTextureFormat format = GPUTextureFormat::kStencil8;
  bool enable_stencil = false;
  GPUStencilState stencil_state = {};
  bool enable_depth = false;
  GPUDepthState depth_state = {};

  bool operator==(const GPUDepthStencilState& other) const {
    return format == other.format && enable_stencil == other.enable_stencil &&
           stencil_state == other.stencil_state &&
           enable_depth == other.enable_depth &&
           depth_state == other.depth_state;
  }

  bool operator!=(const GPUDepthStencilState& other) const {
    return !(*this == other);
  }
};

struct GPUMultisampleState {
  uint32_t count = 1;
};

enum class GPUVertexFormat : uint32_t {
  kFloat32 = 1,
  kFloat32x2 = 2,
  kFloat32x3 = 3,
  kFloat32x4 = 4,
};

enum class GPUVertexStepMode {
  kVertex,
  kInstance,
};

struct GPUVertexAttribute {
  GPUVertexFormat format;
  int64_t offset;
  int32_t shader_location;
};

struct GPUVertexBufferLayout {
  int64_t array_stride;
  GPUVertexStepMode step_mode = GPUVertexStepMode::kVertex;
  std::vector<GPUVertexAttribute> attributes;
};

enum class GPUBlendFactor {
  kZero,
  kOne,
  kSrc,
  kOneMinusSrc,
  kSrcAlpha,
  kOneMinusSrcAlpha,
  kDst,
  kOneMinusDst,
  kDstAlpha,
  kOneMinusDstAlpha,
  kSrcAlphaSaturated,
};

struct GPUColorTargetState {
  GPUTextureFormat format = GPUTextureFormat::kBGRA8Unorm;
  GPUBlendFactor src_blend_factor = GPUBlendFactor::kOne;
  GPUBlendFactor dst_blend_factor = GPUBlendFactor::kOneMinusSrcAlpha;
  int32_t write_mask = 0xF;

  bool operator==(const GPUColorTargetState& other) const {
    return format == other.format &&
           src_blend_factor == other.src_blend_factor &&
           dst_blend_factor == other.dst_blend_factor &&
           write_mask == other.write_mask;
  }
};

struct GPURenderPipelineDescriptor {
  std::shared_ptr<GPUShaderFunction> vertex_function;
  std::shared_ptr<GPUShaderFunction> fragment_function;
  std::vector<GPUVertexBufferLayout> buffers;
  GPUColorTargetState target;
  GPUDepthStencilState depth_stencil = {};
  int32_t sample_count = 1;
  GPUShaderFunctionErrorCallback error_callback;
  std::string label;
};

class GPURenderPipeline {
 public:
  explicit GPURenderPipeline(const GPURenderPipelineDescriptor& desc);

  virtual ~GPURenderPipeline() = default;

  const GPURenderPipelineDescriptor& GetDescriptor() const { return desc_; }

  virtual bool IsValid() const;

  const std::vector<wgx::BindGroup>& GetBindGroups() const {
    return bind_groups_;
  }

  const wgx::BindGroup* GetBindingGroup(uint32_t index) const {
    for (size_t i = 0; i < bind_groups_.size(); i++) {
      if (bind_groups_[i].group == index) {
        return &bind_groups_[i];
      }
    }

    return nullptr;
  }

 private:
  GPURenderPipelineDescriptor desc_;
  // Merged bind groups from vertex and fragment shader functions.
  std::vector<wgx::BindGroup> bind_groups_;
  bool valid_ = true;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_RENDER_PIPELINE_HPP
