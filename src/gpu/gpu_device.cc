// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gpu_device.hpp"

namespace skity {

std::shared_ptr<GPUShaderModule> GPUDevice::CreateShaderModule(
    const GPUShaderModuleDescriptor& desc) {
  return GPUShaderModule::Create(desc);
}

}  // namespace skity
