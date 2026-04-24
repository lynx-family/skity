// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_SHADER_FUNCTION_VK_HPP
#define SRC_GPU_VK_GPU_SHADER_FUNCTION_VK_HPP

#include <vulkan/vulkan.h>

#include "src/gpu/backend_cast.hpp"
#include "src/gpu/gpu_shader_function.hpp"

namespace skity {

class VulkanContextState;

class GPUShaderFunctionVK : public GPUShaderFunction {
 public:
  GPUShaderFunctionVK(GPULabel label, GPUShaderStage stage,
                      std::string entry_point,
                      std::shared_ptr<const VulkanContextState> state,
                      VkShaderModule shader_module);

  ~GPUShaderFunctionVK() override;

  bool IsValid() const override { return shader_module_ != VK_NULL_HANDLE; }

  GPUShaderStage GetStage() const { return stage_; }

  const std::string& GetEntryPoint() const { return entry_point_; }

  VkShaderModule GetShaderModule() const { return shader_module_; }

  SKT_BACKEND_CAST(GPUShaderFunctionVK, GPUShaderFunction)

 private:
  GPUShaderStage stage_ = GPUShaderStage::kVertex;
  std::string entry_point_ = {};
  std::shared_ptr<const VulkanContextState> state_ = {};
  VkShaderModule shader_module_ = VK_NULL_HANDLE;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_SHADER_FUNCTION_VK_HPP
