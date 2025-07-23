// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_SHADER_FUNCTION_MTL_HPP
#define SRC_GPU_MTL_GPU_SHADER_FUNCTION_MTL_HPP

#import <Metal/Metal.h>

#include <vector>

#include "src/gpu/gpu_shader_function.hpp"

namespace skity {
class GPUShaderFunctionMTL : public GPUShaderFunction {
 public:
  GPUShaderFunctionMTL(std::string label, id<MTLDevice> device,
                       GPUShaderStage stage, const char* source,
                       const char* entry_point,
                       const std::vector<int32_t>& constant_values,
                       GPUShaderFunctionErrorCallback error_callback);

  id<MTLFunction> GetMTLFunction() const { return mtl_function_; }

  bool IsValid() const override { return mtl_function_ != nil; }

 private:
  id<MTLFunction> mtl_function_;
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_SHADER_FUNCTION_MTL_HPP
