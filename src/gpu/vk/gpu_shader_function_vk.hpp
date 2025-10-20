// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_SHADER_FUNCTION_VK_HPP
#define SRC_GPU_VK_GPU_SHADER_FUNCTION_VK_HPP

#include <volk.h>

#include <vector>

#include "src/gpu/gpu_shader_function.hpp"

namespace skity {

class GPUDeviceVk;

class GPUShaderFunctionVk : public GPUShaderFunction {
 public:
  GPUShaderFunctionVk(std::string label, GPUShaderStage stage,
                      const std::vector<uint32_t>& spirv_code,
                      const std::vector<int32_t>& constant_values,
                      GPUShaderFunctionErrorCallback error_callback);

  ~GPUShaderFunctionVk() override;

  static std::shared_ptr<GPUShaderFunctionVk> Create(
      GPUDeviceVk* device, const GPUShaderFunctionDescriptor& desc);

  bool Initialize(GPUDeviceVk* device);
  void Destroy();

  VkShaderModule GetShaderModule() const { return shader_module_; }
  VkShaderStageFlagBits GetStage() const { return stage_; }

  bool IsValid() const override { return shader_module_ != VK_NULL_HANDLE; }

 private:
  VkShaderModule shader_module_ = VK_NULL_HANDLE;
  VkShaderStageFlagBits stage_;
  std::vector<uint32_t> spirv_code_;
  std::vector<int32_t> constant_values_;
  GPUDeviceVk* device_ = nullptr;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_SHADER_FUNCTION_VK_HPP
