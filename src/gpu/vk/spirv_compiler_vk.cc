// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/spirv_compiler_vk.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/logging.hpp"

// Configure SPIRV-Cross to work without exceptions
#define SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS

// We'll use a simplified approach for now - in a real implementation
// you would integrate with glslang or use shaderc for GLSL→SPIRV compilation
#include "third_party/SPIRV-Cross/spirv_cross.hpp"
#include "third_party/glslang/SPIRV/GlslangToSpv.h"
#include "third_party/glslang/glslang/Public/ResourceLimits.h"
#include "third_party/glslang/glslang/Public/ShaderLang.h"

namespace skity {

bool SPIRVCompilerVk::glslang_initialized_ = false;

SPIRVCompilerVk::SPIRVCompilerVk(GPUDeviceVk* device) : device_(device) {
  if (!glslang_initialized_) {
    InitializeGlslang();
  }
}

SPIRVCompilerVk::~SPIRVCompilerVk() {
  // Note: We don't finalize glslang here as it's global
  // and other instances might still need it
}

bool SPIRVCompilerVk::InitializeGlslang() {
  if (!glslang_initialized_) {
    glslang::InitializeProcess();
    glslang_initialized_ = true;
    LOGI("glslang initialized for SPIRV compilation");
  }
  return true;
}

void SPIRVCompilerVk::FinalizeGlslang() {
  if (glslang_initialized_) {
    glslang::FinalizeProcess();
    glslang_initialized_ = false;
  }
}

SPIRVCompileResult SPIRVCompilerVk::CompileWGSLToSPIRV(
    const std::string& wgsl_source, const std::string& entry_point,
    GPUShaderStage stage, const SPIRVCompileOptions& options) {
  // Generate cache key and check cache first
  std::string cache_key =
      GenerateCacheKey(wgsl_source, entry_point, stage, options);
  auto cached = GetCachedShader(cache_key);
  if (cached) {
    return *cached;
  }

  SPIRVCompileResult result;

  // Step 1: Convert WGSL to GLSL using existing wgx pipeline
  auto glsl_source = ConvertWGSLToGLSL(wgsl_source, entry_point, stage);
  if (!glsl_source) {
    result.error_message = "Failed to convert WGSL to GLSL";
    return result;
  }

  // Step 2: Compile GLSL to SPIRV
  result = CompileGLSLToSPIRV(*glsl_source, stage, options);

  if (result.success) {
    CacheShader(cache_key, result);
  }

  return result;
}

SPIRVCompileResult SPIRVCompilerVk::CompileGLSLToSPIRV(
    const std::string& glsl_source, GPUShaderStage stage,
    const SPIRVCompileOptions& options) {
  SPIRVCompileResult result;

  // Real implementation using glslang
  EShLanguage glsl_stage = static_cast<EShLanguage>(GetGlslangStage(stage));

  glslang::TShader shader(glsl_stage);
  const char* source_cstr = glsl_source.c_str();
  shader.setStrings(&source_cstr, 1);

  // Set environment
  shader.setEnvInput(glslang::EShSourceGlsl, glsl_stage,
                     glslang::EShClientVulkan, 100);
  shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
  shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

  // Configure default built-in resources
  const TBuiltInResource* resources = GetDefaultResources();

  // Parse shader
  if (!shader.parse(resources, 100, false, EShMsgDefault)) {
    result.error_message =
        "GLSL parse error: " + std::string(shader.getInfoLog());
    return result;
  }

  // Link program
  glslang::TProgram program;
  program.addShader(&shader);

  if (!program.link(EShMsgDefault)) {
    result.error_message =
        "GLSL link error: " + std::string(program.getInfoLog());
    return result;
  }

  // Generate SPIRV
  std::vector<unsigned int> spirv;
  spv::SpvBuildLogger logger;
  glslang::SpvOptions spv_options;
  spv_options.generateDebugInfo = options.debug_info;
  spv_options.disableOptimizer = !options.optimize;
  spv_options.optimizeSize = false;

  glslang::GlslangToSpv(*program.getIntermediate(glsl_stage), spirv, &logger,
                        &spv_options);

  if (!logger.getAllMessages().empty()) {
    LOGW("SPIRV generation warnings: %s", logger.getAllMessages().c_str());
  }

  // Convert to uint32_t vector
  result.spirv_code.assign(spirv.begin(), spirv.end());

  // Extract reflection information
  auto reflection = ReflectSPIRV(result.spirv_code, stage);
  if (reflection) {
    result.reflection = *reflection;
  }

  result.success = true;

  // Validate SPIRV if requested
  if (options.validate && !ValidateSPIRV(result.spirv_code)) {
    result.error_message = "Generated SPIRV failed validation";
    result.success = false;
  }

  return result;
}

std::optional<SPIRVReflectionInfo> SPIRVCompilerVk::ReflectSPIRV(
    const std::vector<uint32_t>& spirv_code, GPUShaderStage stage) {
  spirv_cross::Compiler compiler(spirv_code);
  spirv_cross::ShaderResources resources = compiler.get_shader_resources();

  SPIRVReflectionInfo reflection;
  reflection.stage = stage;

  // Extract uniform buffers
  for (const auto& ubo : resources.uniform_buffers) {
    SPIRVReflectionInfo::UniformBinding binding;
    binding.set = compiler.get_decoration(ubo.id, spv::DecorationDescriptorSet);
    binding.binding = compiler.get_decoration(ubo.id, spv::DecorationBinding);
    binding.name = ubo.name;
    binding.stage = stage;

    // Get size information
    const auto& type = compiler.get_type(ubo.base_type_id);
    binding.size = compiler.get_declared_struct_size(type);

    reflection.uniform_bindings.push_back(binding);
  }

  // Extract texture samplers
  for (const auto& image : resources.sampled_images) {
    SPIRVReflectionInfo::TextureBinding binding;
    binding.set =
        compiler.get_decoration(image.id, spv::DecorationDescriptorSet);
    binding.binding = compiler.get_decoration(image.id, spv::DecorationBinding);
    binding.name = image.name;
    binding.stage = stage;

    reflection.texture_bindings.push_back(binding);
  }

  // Extract separate samplers
  for (const auto& sampler : resources.separate_samplers) {
    SPIRVReflectionInfo::SamplerBinding binding;
    binding.set =
        compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);
    binding.binding =
        compiler.get_decoration(sampler.id, spv::DecorationBinding);
    binding.name = sampler.name;
    binding.stage = stage;

    reflection.sampler_bindings.push_back(binding);
  }

  return reflection;
}

std::optional<std::string> SPIRVCompilerVk::ConvertWGSLToGLSL(
    const std::string& wgsl_source, const std::string& entry_point,
    GPUShaderStage stage) {
  // Use existing wgx pipeline to convert WGSL to GLSL
  auto program = wgx::Program::Parse(wgsl_source);
  if (!program) {
    LOGE("Failed to parse WGSL source");
    return std::nullopt;
  }

  // Configure GLSL options
  wgx::GlslOptions glsl_options;
  glsl_options.standard = wgx::GlslOptions::Standard::kDesktop;
  glsl_options.major_version = 4;
  glsl_options.minor_version = 5;

  auto result = program->WriteToGlsl(entry_point.c_str(), glsl_options);
  if (!result) {
    auto diagnosis = program->GetDiagnosis();
    if (diagnosis) {
      LOGE("WGSL to GLSL conversion failed: %s (line %zu, column %zu)",
           diagnosis->message.c_str(), diagnosis->line, diagnosis->column);
    } else {
      LOGE("WGSL to GLSL conversion failed");
    }
    return std::nullopt;
  }

  return result.content;
}

void SPIRVCompilerVk::CacheShader(const std::string& key,
                                  const SPIRVCompileResult& result) {
  shader_cache_[key] = result;
}

std::optional<SPIRVCompileResult> SPIRVCompilerVk::GetCachedShader(
    const std::string& key) {
  auto it = shader_cache_.find(key);
  if (it != shader_cache_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void SPIRVCompilerVk::ClearCache() { shader_cache_.clear(); }

std::string SPIRVCompilerVk::GenerateCacheKey(
    const std::string& source, const std::string& entry_point,
    GPUShaderStage stage, const SPIRVCompileOptions& options) {
  std::ostringstream oss;
  oss << SPIRVUtils::HashShaderSource(source) << "_" << entry_point << "_"
      << static_cast<int>(stage) << "_" << (options.optimize ? "opt" : "noopt")
      << "_" << (options.debug_info ? "debug" : "nodebug") << "_"
      << (options.validate ? "val" : "noval");

  return oss.str();
}

uint32_t SPIRVCompilerVk::GetGlslangStage(GPUShaderStage stage) {
  switch (stage) {
    case GPUShaderStage::kVertex:
      return EShLangVertex;
    case GPUShaderStage::kFragment:
      return EShLangFragment;
    default:
      return EShLangVertex;
  }
}

bool SPIRVCompilerVk::ValidateSPIRV(const std::vector<uint32_t>& spirv_code) {
  // Basic validation - check magic number and minimum size
  if (spirv_code.size() < 5) {
    return false;
  }

  if (spirv_code[0] != 0x07230203) {  // SPIRV magic number
    return false;
  }

  // For more thorough validation, you would use spirv-val
  // from SPIRV-Tools, but this basic check is sufficient for now
  return true;
}

// SPIRVUtils namespace implementation
namespace SPIRVUtils {

std::string HashShaderSource(const std::string& source) {
  // Simple hash implementation
  std::hash<std::string> hasher;
  size_t hash = hasher(source);

  std::ostringstream oss;
  oss << std::hex << hash;
  return oss.str();
}

const char* ShaderStageToString(GPUShaderStage stage) {
  switch (stage) {
    case GPUShaderStage::kVertex:
      return "vertex";
    case GPUShaderStage::kFragment:
      return "fragment";
    default:
      return "unknown";
  }
}

SPIRVCompileOptions GetDefaultOptions(GPUShaderStage stage) {
  SPIRVCompileOptions options;
  options.optimize = true;
  options.debug_info = false;
  options.validate = true;
  options.target_env_version = 0;  // Automatic

  return options;
}

}  // namespace SPIRVUtils

}  // namespace skity