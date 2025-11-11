// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_pipeline_lib.hpp"

#include "src/gpu/gpu_device.hpp"
#include "src/gpu/gpu_shader_module.hpp"
#include "src/logging.hpp"
#include "src/render/hw/hw_shader_generator.hpp"

namespace skity {

static std::pair<GPUBlendFactor, GPUBlendFactor> get_gpu_blending(
    BlendMode blend_mode) {
  if (blend_mode == BlendMode::kClear) {
    return {GPUBlendFactor::kZero, GPUBlendFactor::kZero};
  } else if (blend_mode == BlendMode::kSrc) {
    return {GPUBlendFactor::kOne, GPUBlendFactor::kZero};
  } else if (blend_mode == BlendMode::kDst) {
    return {GPUBlendFactor::kZero, GPUBlendFactor::kOne};
  } else if (blend_mode == BlendMode::kDstOver) {
    return {GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOne};
  } else if (blend_mode == BlendMode::kSrcIn) {
    return {GPUBlendFactor::kDstAlpha, GPUBlendFactor::kZero};
  } else if (blend_mode == BlendMode::kDstIn) {
    return {GPUBlendFactor::kZero, GPUBlendFactor::kSrcAlpha};
  } else if (blend_mode == BlendMode::kSrcOut) {
    return {GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kZero};
  } else if (blend_mode == BlendMode::kDstOut) {
    return {GPUBlendFactor::kZero, GPUBlendFactor::kOneMinusSrcAlpha};
  } else if (blend_mode == BlendMode::kSrcATop) {
    return {GPUBlendFactor::kDstAlpha, GPUBlendFactor::kOneMinusSrcAlpha};
  } else if (blend_mode == BlendMode::kDstATop) {
    return {GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kSrcAlpha};
  } else if (blend_mode == BlendMode::kXor) {
    return {GPUBlendFactor::kOneMinusDstAlpha,
            GPUBlendFactor::kOneMinusSrcAlpha};
  } else if (blend_mode == BlendMode::kPlus) {
    return {GPUBlendFactor::kOne, GPUBlendFactor::kOne};
  } else {
    return {GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha};
  }
}

static void setup_blending_state(GPURenderPipelineDescriptor &gpu_desc,
                                 const HWPipelineDescriptor &hw_desc) {
  gpu_desc.target.format = hw_desc.color_format;
  gpu_desc.target.write_mask = hw_desc.color_mask;

  auto blend_factor = get_gpu_blending(hw_desc.blend_mode);
  gpu_desc.target.src_blend_factor = blend_factor.first;
  gpu_desc.target.dst_blend_factor = blend_factor.second;

  // need to support other advance blending in the future
}

HWPipeline::HWPipeline(GPUDevice *device,
                       std::unique_ptr<GPURenderPipeline> base_pipeline)
    : gpu_device_(device), gpu_pipelines_() {
  gpu_pipelines_.emplace_back(std::move(base_pipeline));
}

GPURenderPipeline *HWPipeline::GetPipeline(const HWPipelineDescriptor &desc) {
  for (auto &pipeline : gpu_pipelines_) {
    if (PipelineMatch(pipeline.get(), desc)) {
      return pipeline.get();
    }
  }

  auto base_pipeline = gpu_pipelines_.front().get();

  auto gpu_desc = base_pipeline->GetDescriptor();

  setup_blending_state(gpu_desc, desc);
  gpu_desc.depth_stencil = desc.depth_stencil;
  gpu_desc.sample_count = desc.sample_count;

  auto variant_pipeline = gpu_device_->ClonePipeline(base_pipeline, gpu_desc);

  if (!variant_pipeline) {
    return nullptr;
  }

  gpu_pipelines_.emplace_back(std::move(variant_pipeline));

  return gpu_pipelines_.back().get();
}

bool HWPipeline::PipelineMatch(GPURenderPipeline *pipeline,
                               const HWPipelineDescriptor &desc) {
  const auto &gpu_desc = pipeline->GetDescriptor();

  if (gpu_desc.depth_stencil != desc.depth_stencil) {
    return false;
  }

  auto blend_function = get_gpu_blending(desc.blend_mode);

  return gpu_desc.target.write_mask == desc.color_mask &&
         gpu_desc.target.src_blend_factor == blend_function.first &&
         gpu_desc.target.dst_blend_factor == blend_function.second &&
         gpu_desc.sample_count == static_cast<int32_t>(desc.sample_count) &&
         gpu_desc.target.format == desc.color_format;
}

GPURenderPipeline *HWPipelineLib::GetPipeline(
    const HWPipelineKey &key, const HWPipelineDescriptor &desc) {
  auto it = pipelines_.find(key);

  if (it != pipelines_.end()) {
    return it->second->GetPipeline(desc);
  }

  auto pipeline = CreatePipeline(key, desc);

  if (!pipeline) {
    return nullptr;
  }

  auto ret = pipeline->GetPipeline(desc);

  pipelines_.insert({HWPipelineKey(key), std::move(pipeline)});

  return ret;
}

std::unique_ptr<HWPipeline> HWPipelineLib::CreatePipeline(
    const HWPipelineKey &key, const HWPipelineDescriptor &desc) {
  GPURenderPipelineDescriptor gpu_pso_desc{};

  gpu_pso_desc.buffers = desc.buffers;
  gpu_pso_desc.target.format = desc.color_format;
  gpu_pso_desc.sample_count = desc.sample_count;
  gpu_pso_desc.error_callback = [this](char const *message) {
    ctx_->TriggerErrorCallback(GPUError::kPipelineError, message);
  };

  gpu_pso_desc.label = key.frag_name;

  SetupShaderFunction(gpu_pso_desc, key, desc.shader_generator);

  if (!gpu_pso_desc.vertex_function || !gpu_pso_desc.fragment_function) {
    return std::unique_ptr<HWPipeline>();
  }

  setup_blending_state(gpu_pso_desc, desc);
  gpu_pso_desc.depth_stencil = desc.depth_stencil;

  auto gpu_pipeline = gpu_device_->CreateRenderPipeline(gpu_pso_desc);

  if (!gpu_pipeline) {
    return std::unique_ptr<HWPipeline>();
  }

  return std::make_unique<HWPipeline>(gpu_device_, std::move(gpu_pipeline));
}

void HWPipelineLib::SetupShaderFunction(GPURenderPipelineDescriptor &desc,
                                        const HWPipelineKey &key,
                                        HWShaderGenerator *shader_generator) {
  if (shader_generator == nullptr) {
    return;
  }

  wgx::CompilerContext wgx_ctx{};

  desc.vertex_function =
      GetShaderFunction(key.vert_name, GPUShaderStage::kVertex,
                        shader_generator, wgx_ctx, desc.error_callback);
  if (!desc.vertex_function) {
    return;
  }

  wgx_ctx = desc.vertex_function->GetWGXContext();

  desc.fragment_function =
      GetShaderFunction(key.frag_name, GPUShaderStage::kFragment,
                        shader_generator, wgx_ctx, desc.error_callback);
}

std::shared_ptr<GPUShaderFunction> HWPipelineLib::GetShaderFunction(
    const std::string &name, GPUShaderStage stage,
    HWShaderGenerator *shader_generator,
    const wgx::CompilerContext &wgx_context,
    const GPUShaderFunctionErrorCallback &error_callback) {
  if (shader_functions_.count(name)) {
    return shader_functions_[name];
  }

  if (shader_generator == nullptr) {
    return {};
  }

  GPUShaderModuleDescriptor module_desc{};
  module_desc.label = name;
  if (stage == GPUShaderStage::kVertex) {
    module_desc.source = shader_generator->GenVertexWGSL();
    DEBUG_CHECK(!module_desc.source.empty());
  } else if (stage == GPUShaderStage::kFragment) {
    module_desc.source = shader_generator->GenFragmentWGSL();
    DEBUG_CHECK(!module_desc.source.empty());
  } else {
    return {};
  }

  auto module = gpu_device_->CreateShaderModule(module_desc);

  GPUShaderFunctionDescriptor desc{};

  desc.label = name;
  desc.stage = stage;
  desc.error_callback = desc.error_callback;
  desc.source_type = GPUShaderSourceType::kWGX;

  GPUShaderSourceWGX source{};
  source.module = module;
  if (stage == GPUShaderStage::kVertex) {
    source.entry_point = shader_generator->GetVertexEntryPoint();
  } else if (stage == GPUShaderStage::kFragment) {
    source.entry_point = shader_generator->GetFragmentEntryPoint();
  } else {
    return {};
  }
  source.context = wgx_context;

  desc.shader_source = &source;

  auto gpu_shader_function = gpu_device_->CreateShaderFunction(desc);
  if (!gpu_shader_function) {
    return {};
  }
  shader_functions_.insert({name, gpu_shader_function});
  return gpu_shader_function;
}

}  // namespace skity
