// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_GPU_SHADER_FUNCTION_WEB_HPP
#define SRC_GPU_WEB_GPU_SHADER_FUNCTION_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_shader_function.hpp"

namespace skity {

class GPUShaderFunctionWeb : public GPUShaderFunction {
 public:
  GPUShaderFunctionWeb(std::string label, std::string entry_point,
                       WGPUShaderModule shader_module);

  ~GPUShaderFunctionWeb() override;

  bool IsValid() const override { return shader_module_ != nullptr; }

  const std::string& GetEntryPoint() const { return entry_point_; }

  WGPUShaderModule GetShaderModule() const { return shader_module_; }

 private:
  std::string entry_point_;
  WGPUShaderModule shader_module_;
};

}  // namespace skity

#endif  // SRC_GPU_WEB_GPU_SHADER_FUNCTION_WEB_HPP
