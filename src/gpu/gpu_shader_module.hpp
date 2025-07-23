// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_SHADER_MODULE_HPP
#define SRC_GPU_GPU_SHADER_MODULE_HPP

#include <wgsl_cross.h>

namespace skity {

struct GPUShaderModuleDescriptor {
  std::string label = {};
  std::string source = {};
};

/**
 * Shader module just holding the AST for given WGSL source code.
 * The actual shader translation is happend when create GPUShaderFunction from
 * this module.
 */
class GPUShaderModule {
 public:
  GPUShaderModule() = default;

  ~GPUShaderModule() = default;

  static std::shared_ptr<GPUShaderModule> Create(
      const GPUShaderModuleDescriptor& desc);

  const std::string& GetLabel() const { return label_; }

  wgx::Program* GetProgram() const { return program_.get(); }

 private:
  std::string label_ = {};
  std::unique_ptr<wgx::Program> program_ = {};
};

struct GPUShaderSourceWGX {
  std::shared_ptr<GPUShaderModule> module = {};

  const char* entry_point = nullptr;
  wgx::CompilerContext context = {};
};

}  // namespace skity

#endif  // SRC_GPU_GPU_SHADER_MODULE_HPP
