// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_pipeline_lib.hpp"

#include "src/gpu/gpu_caps.hpp"
#include "src/gpu/gpu_device.hpp"
#include "src/gpu/gpu_shader_function.hpp"
#include "src/gpu/gpu_shader_module.hpp"
#include "src/graphic/blend_mode_priv.hpp"
#include "src/logging.hpp"
#include "src/render/hw/hw_pipeline_key.hpp"
#include "src/render/hw/hw_shader_generator.hpp"
#include "src/render/hw/native_blend.hpp"
#include "src/tracing.hpp"

namespace skity {

static std::pair<GPUBlendFactor, GPUBlendFactor> get_gpu_blending(
    BlendMode blend_mode) {
  if (blend_mode == BlendMode::kClear) {
    return {GPUBlendFactor::kZero, GPUBlendFactor::kZero};
  } else if (blend_mode == BlendMode::kSrc) {
    return {GPUBlendFactor::kOne, GPUBlendFactor::kZero};
  } else if (blend_mode == BlendMode::kSrcOver) {
    return {GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha};
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
    return {GPUBlendFactor::kOne, GPUBlendFactor::kZero};
  }
}

struct BlendState {
  GPUBlendFactor src_factor;
  GPUBlendFactor dst_factor;
  GPUBlendOperation op;
};

// programmable_blending packs (blend_mode | dst_read_strategy<<8) when the draw
// reads dst inside the shader (framebuffer-fetch / texture-copy). It is 0 for
// ordinary blends and the kNativeAdvancedBlendKey sentinel for GL's native
// variant; neither of those is shader-side blending.
static bool IsShaderSideBlending(uint32_t programmable_blending) {
  return programmable_blending != 0 &&
         programmable_blending != kNativeAdvancedBlendKey;
}

// Unified blend-state resolution shared by pipeline creation and cache matching
// so they cannot drift apart. Native advanced blend returns
// {kOne, kOneMinusSrcAlpha, native_op}: the GL/Vulkan advanced equations ignore
// the RGB blend factors, while the alpha channel (kept on ADD with those
// factors) computes exactly source-over, matching every advanced shader branch.
//
// shader_side_blending is true for the framebuffer-fetch / texture-copy
// pipeline (programmable_blending holds a packed key): those draws blend inside
// the shader, so the fixed-function equation must stay on ADD regardless of
// mode. For every other pipeline the native equation is selected purely from
// blend_mode + caps — which is what lets the Vulkan native path share one
// pipeline with ordinary blends and differ only by a blend-state variant.
static BlendState resolve_blend_state(BlendMode blend_mode, const GPUCaps& caps,
                                      bool shader_side_blending) {
  if (!shader_side_blending && caps.supports_native_advanced_blend &&
      IsAdvancedBlendMode(blend_mode)) {
    if (auto op = ToNativeBlendOp(blend_mode)) {
      return {GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha, *op};
    }
  }
  auto factors = get_gpu_blending(blend_mode);
  return {factors.first, factors.second, GPUBlendOperation::kAdd};
}

static void setup_blending_state(GPURenderPipelineDescriptor& gpu_desc,
                                 const HWPipelineDescriptor& hw_desc,
                                 const GPUCaps& caps,
                                 bool shader_side_blending) {
  gpu_desc.target.format = hw_desc.color_format;
  gpu_desc.target.write_mask = hw_desc.color_mask;

  auto blend_state =
      resolve_blend_state(hw_desc.blend_mode, caps, shader_side_blending);
  gpu_desc.target.src_blend_factor = blend_state.src_factor;
  gpu_desc.target.dst_blend_factor = blend_state.dst_factor;
  gpu_desc.target.blend_op = blend_state.op;
}

HWPipeline::HWPipeline(GPUDevice* device, GPUBackendType backend,
                       std::unique_ptr<GPURenderPipeline> base_pipeline,
                       bool shader_side_blending)
    : gpu_device_(device),
      backend_(backend),
      gpu_pipelines_(),
      shader_side_blending_(shader_side_blending) {
  gpu_pipelines_.emplace_back(std::move(base_pipeline));
}

GPURenderPipeline* HWPipeline::GetPipeline(const HWPipelineDescriptor& desc) {
  for (auto& pipeline : gpu_pipelines_) {
    if (PipelineMatch(pipeline.get(), desc)) {
      return pipeline.get();
    }
  }

  auto base_pipeline = gpu_pipelines_.front().get();

  auto gpu_desc = base_pipeline->GetDescriptor();

  setup_blending_state(gpu_desc, desc, gpu_device_->GetCaps(),
                       shader_side_blending_);
  gpu_desc.depth_stencil = desc.depth_stencil;
  gpu_desc.sample_count = desc.sample_count;

  auto variant_pipeline = gpu_device_->ClonePipeline(base_pipeline, gpu_desc);

  if (!variant_pipeline) {
    return nullptr;
  }

  gpu_pipelines_.emplace_back(std::move(variant_pipeline));

  return gpu_pipelines_.back().get();
}

bool HWPipeline::PipelineMatch(GPURenderPipeline* pipeline,
                               const HWPipelineDescriptor& desc) {
  if (!DepthStencilStateMatch(pipeline, desc)) {
    return false;
  }

  const auto& gpu_desc = pipeline->GetDescriptor();

  auto blend_state = resolve_blend_state(
      desc.blend_mode, gpu_device_->GetCaps(), shader_side_blending_);

  return gpu_desc.target.write_mask == desc.color_mask &&
         gpu_desc.target.src_blend_factor == blend_state.src_factor &&
         gpu_desc.target.dst_blend_factor == blend_state.dst_factor &&
         gpu_desc.target.blend_op == blend_state.op &&
         gpu_desc.sample_count == static_cast<int32_t>(desc.sample_count) &&
         gpu_desc.target.format == desc.color_format;
}

bool HWPipeline::DepthStencilStateMatch(GPURenderPipeline* pipeline,
                                        const HWPipelineDescriptor& desc) {
  const auto& gpu_desc = pipeline->GetDescriptor();
  if (backend_ != GPUBackendType::kVulkan) {
    return gpu_desc.depth_stencil == desc.depth_stencil;
  }

  const auto& pipeline_ds = gpu_desc.depth_stencil;
  const auto& request_ds = desc.depth_stencil;
  if (pipeline_ds.format != request_ds.format ||
      pipeline_ds.enable_stencil != request_ds.enable_stencil ||
      pipeline_ds.enable_depth != request_ds.enable_depth ||
      !(pipeline_ds.depth_state == request_ds.depth_state)) {
    return false;
  }

  const auto stencil_face_match = [](const GPUStencilFaceState& lhs,
                                     const GPUStencilFaceState& rhs) -> bool {
    return lhs.compare == rhs.compare && lhs.fail_op == rhs.fail_op &&
           lhs.depth_fail_op == rhs.depth_fail_op && lhs.pass_op == rhs.pass_op;
  };

  return stencil_face_match(pipeline_ds.stencil_state.front,
                            request_ds.stencil_state.front) &&
         stencil_face_match(pipeline_ds.stencil_state.back,
                            request_ds.stencil_state.back);
}

GPURenderPipeline* HWPipelineLib::GetPipeline(
    const HWPipelineKey& key, const HWPipelineDescriptor& desc) {
#ifdef SKITY_ENABLE_TRACING
  uint64_t vs_key = (static_cast<uint64_t>(GPUShaderStage::kVertex) << 32) |
                    key.GetVertexBaseKey();
  uint64_t fs_key = (static_cast<uint64_t>(GPUShaderStage::kFragment) << 32) |
                    key.GetFragmentBaseKey();
  std::string vs_name = FunctionBaseKeyToShaderName(vs_key);
  std::string fs_name = FunctionBaseKeyToShaderName(fs_key);
  SKITY_TRACE_EVENT_ARGS(HWPipelineLib_GetPipeline, "vs", vs_name.c_str(), "fs",
                         fs_name.c_str());
#endif
  auto it = pipelines_.find(key);

  if (it != pipelines_.end()) {
    return it->second->GetPipeline(desc);
  }

  // If the pipeline has previously failed to compile, don't retry in the
  // current frame.
  if (compile_failed_pipelines_.find(key) != compile_failed_pipelines_.end()) {
    return nullptr;
  }

  auto pipeline = CreatePipeline(key, desc);

  if (!pipeline) {
    LOGE("CreatePipeline failed, vs: {} fs: {}",
         VertexKeyToShaderName(key.GetVertexBaseKey()),
         FragmentKeyToShaderName(key.GetFragmentBaseKey(), key.compose_keys));
    compile_failed_pipelines_.insert(key);
    return nullptr;
  }

  auto ret = pipeline->GetPipeline(desc);

  pipelines_.insert({HWPipelineKey(key), std::move(pipeline)});

  return ret;
}

std::unique_ptr<HWPipeline> HWPipelineLib::CreatePipeline(
    const HWPipelineKey& key, const HWPipelineDescriptor& desc) {
  DEBUG_CHECK(desc.shader_generator);
  GPURenderPipelineDescriptor gpu_pso_desc{};

  gpu_pso_desc.buffers = desc.buffers;
  gpu_pso_desc.target.format = desc.color_format;
  gpu_pso_desc.sample_count = desc.sample_count;
  gpu_pso_desc.error_callback = [this](char const* message) {
    ctx_->TriggerErrorCallback(GPUError::kPipelineError, message);
  };
  gpu_pso_desc.label =
      GPULabel(key.GetFunctionKey(GPUShaderStage::kFragment).base_key,
               FunctionBaseKeyToShaderName);

  SetupShaderFunction(gpu_pso_desc, key, desc.shader_generator);

  if (!gpu_pso_desc.vertex_function || !gpu_pso_desc.fragment_function) {
    return std::unique_ptr<HWPipeline>();
  }

  bool shader_side_blending = IsShaderSideBlending(key.programmable_blending);

  setup_blending_state(gpu_pso_desc, desc, gpu_device_->GetCaps(),
                       shader_side_blending);
  gpu_pso_desc.depth_stencil = desc.depth_stencil;

  auto gpu_pipeline = gpu_device_->CreateRenderPipeline(gpu_pso_desc);

  if (!gpu_pipeline) {
    return std::unique_ptr<HWPipeline>();
  }

  return std::make_unique<HWPipeline>(
      gpu_device_, backend_, std::move(gpu_pipeline), shader_side_blending);
}

void HWPipelineLib::SetupShaderFunction(GPURenderPipelineDescriptor& desc,
                                        const HWPipelineKey& key,
                                        HWShaderGenerator* shader_generator) {
  if (shader_generator == nullptr) {
    return;
  }

  wgx::CompilerContext wgx_ctx{};

  desc.vertex_function =
      GetShaderFunction(key, GPUShaderStage::kVertex, shader_generator, wgx_ctx,
                        desc.error_callback);

  if (!desc.vertex_function) {
    return;
  }

  wgx_ctx = desc.vertex_function->GetWGXContext();

  desc.fragment_function =
      GetShaderFunction(key, GPUShaderStage::kFragment, shader_generator,
                        wgx_ctx, desc.error_callback);
}

std::shared_ptr<GPUShaderFunction> HWPipelineLib::GetShaderFunction(
    const HWPipelineKey& pipeline_key, GPUShaderStage stage,
    HWShaderGenerator* shader_generator,
    const wgx::CompilerContext& wgx_context,
    const GPUShaderFunctionErrorCallback& error_callback) {
  HWFunctionKey function_key = pipeline_key.GetFunctionKey(stage);
  if (shader_functions_.count(function_key)) {
    return shader_functions_[function_key];
  }

  if (shader_generator == nullptr) {
    return {};
  }

  GPUShaderModuleDescriptor module_desc{};

  module_desc.label =
      GPULabel(function_key.base_key, FunctionBaseKeyToShaderName);

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

  desc.label = module_desc.label;
  desc.stage = stage;
  desc.error_callback = error_callback;
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

  // Native-blend fragment shaders must be translated with the
  // GL_KHR_blend_equation_advanced extension injected; signal that through the
  // source. The sentinel lives only in the fragment key (GetFunctionKey does
  // not fold programmable_blending into the vertex key), so gate on fragment.
  if (stage == GPUShaderStage::kFragment &&
      pipeline_key.programmable_blending == kNativeAdvancedBlendKey) {
    source.needs_native_advanced_blend = true;
  }

  desc.shader_source = &source;

  auto gpu_shader_function = gpu_device_->CreateShaderFunction(desc);
  if (!gpu_shader_function) {
    return {};
  }
  shader_functions_.insert({function_key, gpu_shader_function});
  return gpu_shader_function;
}

}  // namespace skity
