// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/gpu_device_gl.hpp"

#include <cstring>
#include <string_view>

#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gl/gpu_buffer_gl.hpp"
#include "src/gpu/gl/gpu_command_buffer_gl.hpp"
#include "src/gpu/gl/gpu_render_pipeline_gl.hpp"
#include "src/gpu/gl/gpu_sampler_gl.hpp"
#include "src/gpu/gl/gpu_shader_function_gl.hpp"
#include "src/gpu/gl/gpu_texture_gl.hpp"
#include "src/tracing.hpp"

namespace skity {
namespace {

constexpr int ParseDecimal(std::string_view value, size_t offset) {
  if (offset >= value.size() || value[offset] < '0' || value[offset] > '9') {
    return -1;
  }

  int result = 0;
  while (offset < value.size() && value[offset] >= '0' &&
         value[offset] <= '9') {
    result = result * 10 + value[offset++] - '0';
  }
  return result;
}

constexpr int GetAdrenoModel(std::string_view renderer) {
  constexpr std::string_view kAdrenoPrefix = "Adreno (TM) ";
  auto offset = renderer.find(kAdrenoPrefix);
  if (offset != std::string_view::npos) {
    return ParseDecimal(renderer, offset + kAdrenoPrefix.size());
  }

  if (renderer.size() >= 2 && renderer[0] == 'F' && renderer[1] == 'D') {
    return ParseDecimal(renderer, 2);
  }
  return -1;
}

constexpr bool HasBrokenFramebufferFetch(std::string_view renderer) {
  // Adreno 5xx advertises GL_EXT_shader_framebuffer_fetch, but fetching the
  // destination is known to produce incorrect results (for example, Galaxy
  // S7). The extension must not be exposed as a usable capability.
  int adreno_model = GetAdrenoModel(renderer);
  return adreno_model >= 500 && adreno_model < 600;
}

constexpr bool HasBrokenNativeAdvancedBlend(std::string_view vendor,
                                            std::string_view renderer,
                                            std::string_view version) {
  // Skia disables KHR advanced blend equations on these driver families after
  // observing major correctness issues. Prefer the programmable fallback even
  // when the driver advertises the extension.
  int adreno_model = GetAdrenoModel(renderer);
  bool is_intel_driver = vendor.find("Intel") != std::string_view::npos &&
                         version.find("Mesa") == std::string_view::npos;
  bool is_angle_intel = renderer.find("ANGLE") != std::string_view::npos &&
                        renderer.find("Intel") != std::string_view::npos;
  return (adreno_model >= 400 && adreno_model < 600) ||
         vendor.find("ARM") != std::string_view::npos || is_intel_driver ||
         is_angle_intel;
}

std::string_view GetGLString(uint32_t name) {
  auto* value = GL_CALL(GetString, name);
  return value == nullptr
             ? std::string_view{}
             : std::string_view{reinterpret_cast<const char*>(value)};
}

static_assert(HasBrokenFramebufferFetch("Adreno (TM) 530"));
static_assert(!HasBrokenFramebufferFetch("Adreno (TM) 640"));
static_assert(HasBrokenNativeAdvancedBlend("Qualcomm", "Adreno (TM) 430", ""));
static_assert(HasBrokenNativeAdvancedBlend("ARM", "Mali-G76", ""));
static_assert(HasBrokenNativeAdvancedBlend(
    "Google Inc.", "ANGLE (Intel, Intel UHD Graphics, OpenGL)", ""));
static_assert(HasBrokenNativeAdvancedBlend("Intel Inc.",
                                           "Intel Iris OpenGL Engine",
                                           "4.1 INTEL-20.1.8"));
static_assert(!HasBrokenNativeAdvancedBlend("Intel", "Mesa Intel(R) Graphics",
                                            "4.6 Mesa 24.0"));
static_assert(!HasBrokenNativeAdvancedBlend("Qualcomm", "Adreno (TM) 640", ""));

}  // namespace

GPUDeviceGL::GPUDeviceGL() {
  InitGLVersion();
  auto gpu_caps = std::make_unique<GPUCaps>();
  auto* gl_interface = GLInterface::GlobalInterface();
  auto vendor = GetGLString(GL_VENDOR);
  auto renderer = GetGLString(GL_RENDERER);
  auto version = GetGLString(GL_VERSION);

  // An advertised extension is not sufficient on these drivers. Match Skia's
  // policies so the resolver rejects known-broken framebuffer-fetch and native
  // advanced-blend paths and chooses a safe programmable fallback.
  gpu_caps->supports_framebuffer_fetch =
      gl_interface->ext_shader_framebuffer_fetch &&
      !HasBrokenFramebufferFetch(renderer);
  bool supports_native_advanced_blend =
      gl_interface->ext_khr_blend_equation_advanced &&
      !HasBrokenNativeAdvancedBlend(vendor, renderer, version);
  gpu_caps->supports_native_advanced_blend = supports_native_advanced_blend;
  gpu_caps->supports_native_advanced_blend_coherent =
      supports_native_advanced_blend &&
      gl_interface->ext_khr_blend_equation_advanced_coherent;
  gpu_caps->native_blend_shader_variant = supports_native_advanced_blend;
  gpu_caps->supports_dual_source_blending =
      is_gles_ ? gl_interface->ext_blend_func_extended
               : gl_version_major_ > 3 ||
                     (gl_version_major_ == 3 && gl_version_minor_ >= 3);
  InitCaps(std::move(gpu_caps));
}

GPUDeviceGL::~GPUDeviceGL() = default;

std::unique_ptr<GPUBuffer> GPUDeviceGL::CreateBuffer(
    const GPUBufferDescriptor& desc) {
  return std::make_unique<GPUBufferGL>(desc);
}

std::shared_ptr<GPUShaderFunction> GPUDeviceGL::CreateShaderFunction(
    const GPUShaderFunctionDescriptor& desc) {
  if (desc.features.dual_source_blending &&
      !GetCaps().supports_dual_source_blending) {
    return {};
  }

  if (desc.source_type == GPUShaderSourceType::kWGX) {
    return CreateShaderFunctionFromModule(desc);
  }

  const GPUShaderSourceRaw* source =
      reinterpret_cast<const GPUShaderSourceRaw*>(desc.shader_source);

  auto function = std::make_shared<GPUShaderFunctionGL>(
      desc.label, desc.stage, source->source, desc.error_callback);

  if (!function->IsValid()) {
    return nullptr;
  }

  // If using pre-compiled shader
  // OpenGL version is 330 core
  // OpenGLES version is 300 es
  if (is_gles_) {
    function->SetupGLVersion(3, 0, true);
  } else {
    function->SetupGLVersion(3, 3, false);
  }

  return function;
}

std::unique_ptr<GPURenderPipeline> GPUDeviceGL::CreateRenderPipeline(
    const GPURenderPipelineDescriptor& desc) {
  if (UsesDualSourceBlending(desc.target) &&
      !GetCaps().supports_dual_source_blending) {
    return {};
  }
  auto pipeline = std::make_unique<GPURenderPipelineGL>(desc);
  if (!pipeline->IsValid()) {
    return nullptr;
  }
  return pipeline;
}

std::unique_ptr<GPURenderPipeline> GPUDeviceGL::ClonePipeline(
    GPURenderPipeline* base, const GPURenderPipelineDescriptor& desc) {
  if (UsesDualSourceBlending(desc.target) &&
      !GetCaps().supports_dual_source_blending) {
    return {};
  }
  if (!base->IsValid()) {
    return std::unique_ptr<GPURenderPipeline>();
  }

  // OpenGL don't need to recreate pipeline if stencil and blending changes
  auto pipeline_gl = static_cast<GPURenderPipelineGL*>(base);

  return std::make_unique<GPURenderPipelineGL>(pipeline_gl->GetProgram(), desc);
}

std::shared_ptr<GPUCommandBuffer> GPUDeviceGL::CreateCommandBuffer() {
  return std::make_shared<GPUCommandBufferGL>(CanUseMSAA());
}

std::shared_ptr<GPUSampler> GPUDeviceGL::CreateSampler(
    const GPUSamplerDescriptor& desc) {
  auto it = sampler_map_.find(desc);
  if (it != sampler_map_.end()) {
    return it->second;
  }
  auto sampler = GPUSamplerGL::Create(desc);
  sampler_map_.insert({desc, sampler});
  return sampler;
}

std::shared_ptr<GPUTexture> GPUDeviceGL::CreateTexture(
    const GPUTextureDescriptor& desc) {
#ifdef SKITY_ANDROID
  // if running on Android, and the msaa extension is supported, create a fake
  // msaa texture for GPUPipelineDescriptor
  if (desc.storage_mode == GPUTextureStorageMode::kMemoryless &&
      desc.sample_count > 1 && desc.format != GPUTextureFormat::kStencil8 &&
      desc.format != GPUTextureFormat::kDepth24Stencil8 &&
      GLInterface::GlobalInterface()->ext_multisampled_render_to_texture) {
    return std::make_shared<GPUTexturePlaceholderGL>(desc);
  }
#endif

  // Texture only support depth_and_stencil combined format
  // but render buffer can support stencil only format
  // to save some memory, in GL backend, we use RenderBuffer in stencil
  // attachment
  if (desc.format == GPUTextureFormat::kStencil8 ||
      desc.format == GPUTextureFormat::kDepth24Stencil8) {
    return GPUTextureRenderBufferGL::Create(desc);
  } else if (desc.sample_count > 1) {
    // use renderbuffer to optimize color attachment
    return GPUTextureRenderBufferGL::Create(desc);
  }

  return GPUTextureGL::Create(desc);
}

bool GPUDeviceGL::CanUseMSAA() {
  return GLInterface::GlobalInterface()->CanUseMSAA();
}

uint32_t GPUDeviceGL::GetBufferAlignment() {
  if (ubo_offset_ == 0) {
    GLint offset = 0;
    GL_CALL(GetIntegerv, GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &offset);

    ubo_offset_ = offset;
  }

  if (ubo_offset_ == 0) {
    // Fix div0 crash when driver not return currect value
    ubo_offset_ = 256;
  }

  return ubo_offset_;
}

uint32_t GPUDeviceGL::GetMaxTextureSize() {
  if (max_texture_size_ == 0) {
    GLint max_size = 0;
    GL_CALL(GetIntegerv, GL_MAX_TEXTURE_SIZE, &max_size);
    max_texture_size_ = max_size;
  }

  return max_texture_size_;
}

std::shared_ptr<GPUShaderFunction> GPUDeviceGL::CreateShaderFunctionFromModule(
    const GPUShaderFunctionDescriptor& desc) {
  SKITY_TRACE_EVENT(GPUDeviceGL_CreateShaderFunctionFromModuleWGX);

  if (desc.source_type != GPUShaderSourceType::kWGX) {
    return {};
  }

  GPUShaderSourceWGX* source =
      reinterpret_cast<GPUShaderSourceWGX*>(desc.shader_source);

  if (!source->module || source->module->GetProgram() == nullptr ||
      source->entry_point == nullptr) {
    return {};
  }

  wgx::GlslOptions options{};

  options.standard = is_gles_ ? wgx::GlslOptions::Standard::kES
                              : wgx::GlslOptions::Standard::kDesktop;
  options.major_version = gl_version_major_;
  options.minor_version = gl_version_minor_;
  if (desc.features.native_advanced_blend) {
    options.extensions.push_back("GL_KHR_blend_equation_advanced");
  }
  if (desc.features.framebuffer_fetch) {
    options.extensions.push_back("GL_EXT_shader_framebuffer_fetch");
  }
  if (is_gles_ && desc.features.dual_source_blending) {
    options.extensions.push_back("GL_EXT_blend_func_extended");
  }

  auto wgx_result = source->module->GetProgram()->WriteToGlsl(
      source->entry_point, options, source->context);

  if (!wgx_result.success) {
    if (desc.error_callback) {
      desc.error_callback("WGX translate error");
    }

    return {};
  }

  LOGD("WGX shader_module ( {} ) translate function ( {} ) result:\n{}",
       source->module->GetLabel(), source->entry_point, wgx_result.content);

  auto function = std::make_shared<GPUShaderFunctionGL>(
      desc.label, desc.stage, wgx_result.content.c_str(), desc.error_callback);

  if (!function->IsValid()) {
    return {};
  }

  function->SetBindGroups(wgx_result.bind_groups);
  function->SetWGXContext(wgx_result.context);

  // pass the wgx context to caller
  source->context = wgx_result.context;

  function->SetupGLVersion(gl_version_major_, gl_version_minor_, is_gles_);

  return function;
}

void GPUDeviceGL::InitGLVersion() {
  GL_CALL(GetIntegerv, GL_MAJOR_VERSION, &gl_version_major_);
  GL_CALL(GetIntegerv, GL_MINOR_VERSION, &gl_version_minor_);

  auto version = GL_CALL(GetString, GL_VERSION);

  if (std::strstr(reinterpret_cast<const char*>(version), "OpenGL ES") !=
      nullptr) {
    is_gles_ = true;
  }
}

}  // namespace skity
