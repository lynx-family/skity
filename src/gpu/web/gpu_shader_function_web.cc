// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_shader_function_web.hpp"

namespace skity {

GPUShaderFunctionWeb::GPUShaderFunctionWeb(std::string label,
                                           std::string entry_point,
                                           WGPUShaderModule shader_module)
    : GPUShaderFunction(std::move(label)),
      entry_point_(std::move(entry_point)),
      shader_module_(shader_module) {
  wgpuShaderModuleAddRef(shader_module_);
}

GPUShaderFunctionWeb::~GPUShaderFunctionWeb() {
  wgpuShaderModuleRelease(shader_module_);
}

}  // namespace skity
