// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Stub implementations for Vulkan GPU context functions.
// These are compiled when SKITY_VK_BACKEND is OFF to provide linkable symbols
// that return nullptr, preventing link errors and runtime SO loading failures.

#include <skity/gpu/gpu_context_vk.hpp>

namespace skity {

std::unique_ptr<GPUContext> CreateGPUContextVK(const GPUContextInfoVK* info) {
  (void)info;
  return {};
}

std::unique_ptr<GPUContext> CreateGPUContextVK(
    PFN_vkGetInstanceProcAddr get_instance_proc_addr) {
  (void)get_instance_proc_addr;
  return {};
}

std::unique_ptr<GPUNativeWindowVK> CreateGPUNativeWindowVK(
    GPUContext* context, const GPUNativeWindowInfoVK* info) {
  (void)context;
  (void)info;
  return {};
}

}  // namespace skity
