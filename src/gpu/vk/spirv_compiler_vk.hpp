// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_SPIRV_COMPILER_VK_HPP
#define SRC_GPU_VK_SPIRV_COMPILER_VK_HPP

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/gpu/gpu_shader_function.hpp"
#include "wgsl_cross.h"

namespace skity {

class GPUDeviceVk;

struct SPIRVCompileOptions {
  bool optimize = true;
  bool debug_info = false;
  bool validate = true;
  uint32_t target_env_version = 0;  // 0 = automatic detection
};

struct SPIRVReflectionInfo {
  struct UniformBinding {
    uint32_t set;
    uint32_t binding;
    std::string name;
    size_t size;
    GPUShaderStage stage;
  };

  struct TextureBinding {
    uint32_t set;
    uint32_t binding;
    std::string name;
    GPUShaderStage stage;
  };

  struct SamplerBinding {
    uint32_t set;
    uint32_t binding;
    std::string name;
    GPUShaderStage stage;
  };

  std::vector<UniformBinding> uniform_bindings;
  std::vector<TextureBinding> texture_bindings;
  std::vector<SamplerBinding> sampler_bindings;

  // Entry point information
  std::string entry_point;
  GPUShaderStage stage;

  // Vertex input attributes (for vertex shaders)
  struct VertexAttribute {
    uint32_t location;
    std::string name;
    uint32_t format;  // VkFormat
  };
  std::vector<VertexAttribute> vertex_attributes;

  // Push constant information
  struct PushConstant {
    uint32_t offset;
    uint32_t size;
    GPUShaderStage stage;
  };
  std::optional<PushConstant> push_constant;
};

struct SPIRVCompileResult {
  std::vector<uint32_t> spirv_code;
  SPIRVReflectionInfo reflection;
  bool success = false;
  std::string error_message;

  operator bool() const { return success; }
};

/**
 * SPIRV Compiler for Vulkan backend
 *
 * This class integrates with the existing WGSL→GLSL pipeline and adds
 * GLSL→SPIRV compilation capabilities for Vulkan shaders.
 *
 * Pipeline: WGSL → GLSL → SPIRV (via glslang)
 */
class SPIRVCompilerVk {
 public:
  explicit SPIRVCompilerVk(GPUDeviceVk* device);
  ~SPIRVCompilerVk();

  /**
   * Compile WGSL source to SPIRV bytecode
   *
   * @param wgsl_source The WGSL shader source code
   * @param entry_point The shader entry point function name
   * @param stage The shader stage (vertex, fragment, etc.)
   * @param options Compilation options
   * @return Compilation result with SPIRV bytecode and reflection info
   */
  SPIRVCompileResult CompileWGSLToSPIRV(
      const std::string& wgsl_source, const std::string& entry_point,
      GPUShaderStage stage, const SPIRVCompileOptions& options = {});

  /**
   * Compile GLSL source to SPIRV bytecode (direct compilation)
   *
   * @param glsl_source The GLSL shader source code
   * @param stage The shader stage
   * @param options Compilation options
   * @return Compilation result with SPIRV bytecode and reflection info
   */
  SPIRVCompileResult CompileGLSLToSPIRV(
      const std::string& glsl_source, GPUShaderStage stage,
      const SPIRVCompileOptions& options = {});

  /**
   * Get reflection information from compiled SPIRV bytecode
   *
   * @param spirv_code The compiled SPIRV bytecode
   * @param stage The shader stage
   * @return Reflection information for descriptor set binding
   */
  std::optional<SPIRVReflectionInfo> ReflectSPIRV(
      const std::vector<uint32_t>& spirv_code, GPUShaderStage stage);

  /**
   * Cache compiled shaders for reuse
   *
   * @param key Unique identifier for the shader
   * @param result Compilation result to cache
   */
  void CacheShader(const std::string& key, const SPIRVCompileResult& result);

  /**
   * Retrieve cached shader
   *
   * @param key Unique identifier for the shader
   * @return Cached compilation result if found
   */
  std::optional<SPIRVCompileResult> GetCachedShader(const std::string& key);

  /**
   * Clear shader cache
   */
  void ClearCache();

  /**
   * Initialize glslang (called automatically)
   */
  static bool InitializeGlslang();

  /**
   * Finalize glslang (called automatically)
   */
  static void FinalizeGlslang();

 private:
  GPUDeviceVk* device_;

  // Shader cache for compiled SPIRV
  std::unordered_map<std::string, SPIRVCompileResult> shader_cache_;

  /**
   * Convert WGSL to GLSL using existing wgx pipeline
   */
  std::optional<std::string> ConvertWGSLToGLSL(const std::string& wgsl_source,
                                               const std::string& entry_point,
                                               GPUShaderStage stage);

  /**
   * Get GLSL version string for shader stage
   */
  std::string GetGLSLVersionString(GPUShaderStage stage);

  /**
   * Generate shader cache key
   */
  std::string GenerateCacheKey(const std::string& source,
                               const std::string& entry_point,
                               GPUShaderStage stage,
                               const SPIRVCompileOptions& options);

  /**
   * Extract binding information from wgx result
   */
  void ExtractBindingInfo(const wgx::Result& wgx_result,
                          SPIRVReflectionInfo& reflection);

  /**
   * Convert GPU shader stage to glslang stage
   */
  uint32_t GetGlslangStage(GPUShaderStage stage);

  /**
   * Validate SPIRV bytecode
   */
  bool ValidateSPIRV(const std::vector<uint32_t>& spirv_code);

  static bool glslang_initialized_;
};

/**
 * Helper functions for SPIRV processing
 */
namespace SPIRVUtils {

/**
 * Generate a unique hash for shader source
 */
std::string HashShaderSource(const std::string& source);

/**
 * Convert GPU shader stage to string
 */
const char* ShaderStageToString(GPUShaderStage stage);

/**
 * Get default SPIRV compile options for a stage
 */
SPIRVCompileOptions GetDefaultOptions(GPUShaderStage stage);

}  // namespace SPIRVUtils

}  // namespace skity

#endif  // SRC_GPU_VK_SPIRV_COMPILER_VK_HPP