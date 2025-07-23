// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_PIPELINE_LIB_HPP
#define SRC_RENDER_HW_HW_PIPELINE_LIB_HPP

#include <wgsl_cross.h>

#include <memory>
#include <skity/gpu/gpu_context.hpp>
#include <skity/graphic/blend_mode.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/gpu/gpu_render_pipeline.hpp"

namespace skity {

class GPUDevice;
class HWShaderGenerator;

/**
 * High level abstraction about GPURenderPipelineDescriptor
 * The reason we create a new struct instead of using
 * GPURenderPipelineDescriptor directly is that: We don't want the HWDraw
 * implementation knows about GPUShaderFunction and other pipeline layout
 * information, and hoping the shader-reflection can provide these information
 * in the future
 */
struct HWPipelineDescriptor {
  int32_t color_mask = 0xF;
  uint32_t sample_count = 1;
  std::vector<GPUVertexBufferLayout> buffers = {};
  BlendMode blend_mode = BlendMode::kDefault;
  GPUTextureFormat color_format = GPUTextureFormat::kRGBA8Unorm;
  GPUDepthStencilState depth_stencil = {};
  HWShaderGenerator* shader_generator = nullptr;
};

class HWPipeline {
 public:
  HWPipeline(GPUDevice* device,
             std::unique_ptr<GPURenderPipeline> base_pipeline);

  ~HWPipeline() = default;

  GPURenderPipeline* GetPipeline(const HWPipelineDescriptor& desc);

 private:
  bool PipelineMatch(GPURenderPipeline* pipeline,
                     const HWPipelineDescriptor& desc);

 private:
  GPUDevice* gpu_device_;
  std::vector<std::unique_ptr<GPURenderPipeline>> gpu_pipelines_;
};

struct HWPipelineKey {
  std::string vert_name;
  std::string frag_name;
  std::vector<int32_t> vert_constant_values;
  std::vector<int32_t> frag_constant_values;

  bool operator==(const HWPipelineKey& other) const {
    return vert_name == other.vert_name && frag_name == other.frag_name &&
           vert_constant_values == other.vert_constant_values &&
           frag_constant_values == other.frag_constant_values;
  }
};

struct HWPipelineKeyHash {
  std::size_t operator()(const HWPipelineKey& key) const {
    size_t res = 17;
    res += std::hash<std::string>()(key.vert_name);
    for (int32_t value : key.vert_constant_values) {
      res += std::hash<int32_t>()(value);
    }
    res += std::hash<std::string>()(key.frag_name);
    for (int32_t value : key.frag_constant_values) {
      res += std::hash<int32_t>()(value);
    }
    return res;
  }
};

class HWPipelineLib final {
  using PipelineMap =
      std::unordered_map<HWPipelineKey, std::unique_ptr<HWPipeline>,
                         HWPipelineKeyHash>;

  using ShaderFunctionCache =
      std::unordered_map<std::string, std::shared_ptr<GPUShaderFunction>>;

 public:
  HWPipelineLib(GPUContext* ctx, GPUBackendType backend, GPUDevice* device)
      : ctx_(ctx), backend_(backend), gpu_device_(device) {}

  ~HWPipelineLib() = default;

  GPURenderPipeline* GetPipeline(const HWPipelineKey& key,
                                 const HWPipelineDescriptor& desc);

 private:
  std::unique_ptr<HWPipeline> CreatePipeline(const HWPipelineKey& key,
                                             const HWPipelineDescriptor& desc);

  void SetupShaderFunction(GPURenderPipelineDescriptor& desc,
                           const HWPipelineKey& key,
                           HWShaderGenerator* shader_generator);

  std::shared_ptr<GPUShaderFunction> GetShaderFunction(
      const std::string& name, GPUShaderStage stage,
      HWShaderGenerator* shader_generator,
      const wgx::CompilerContext& wgx_context,
      const GPUShaderFunctionErrorCallback& error_callback);

 private:
  GPUContext* ctx_;
  GPUBackendType backend_;
  GPUDevice* gpu_device_;
  PipelineMap pipelines_ = {};
  ShaderFunctionCache shader_functions_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_PIPELINE_LIB_HPP
