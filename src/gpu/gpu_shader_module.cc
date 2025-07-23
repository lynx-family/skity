// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gpu_shader_module.hpp"

#include "src/logging.hpp"
#include "src/tracing.hpp"

namespace skity {

std::shared_ptr<GPUShaderModule> GPUShaderModule::Create(
    const GPUShaderModuleDescriptor& desc) {
  SKITY_TRACE_EVENT(GPUShaderModule_CreateWGX);

  auto program = wgx::Program::Parse(desc.source);

  if (program->GetDiagnosis()) {
    auto diagnosis = *(program->GetDiagnosis());

    LOGE("WGX: Failed to parse shader source > {} <, at {}:{} error : {}",
         desc.label, diagnosis.line, diagnosis.column, diagnosis.message);

    return nullptr;
  }

  auto module = std::make_shared<GPUShaderModule>();
  module->label_ = desc.label;
  module->program_ = std::move(program);

  return module;
}

}  // namespace skity
