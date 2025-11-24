// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_WEB_GPU_SHADER_MODULE_WEB_HPP
#define SRC_GPU_WEB_GPU_SHADER_MODULE_WEB_HPP

#include <webgpu/webgpu.h>

#include "src/gpu/gpu_shader_module.hpp"

namespace skity {

class GPUShaderModuleWEB : public GPUShaderModule {
 public:
  GPUShaderModuleWEB(GPUShaderModule& base, WGPUShaderModule shader_module);

  ~GPUShaderModuleWEB() override;

  WGPUShaderModule GetShaderModule() const { return shader_module_; }

  static std::shared_ptr<GPUShaderModule> Create(
      WGPUDevice device, const GPUShaderModuleDescriptor& desc);

 private:
  WGPUShaderModule shader_module_ = nullptr;
};

}  // namespace skity

#endif  // SRC_GPU_WEB_GPU_SHADER_MODULE_WEB_HPP
