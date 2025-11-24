// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_CONTEXT_WEB_HPP
#define INCLUDE_SKITY_GPU_GPU_CONTEXT_WEB_HPP

#include <webgpu/webgpu.h>

#include <skity/gpu/gpu_context.hpp>

namespace skity {

struct GPUSurfaceDescriptorWEB : public GPUSurfaceDescriptor {
  /**
   * @brief WebGPU texture that will be used as the render target.
   *
   * @note The texture must have usage WGPUTextureUsage_RenderAttachment.
   */
  WGPUTexture texture = {};
};

/**
 * Create a WebGPU backend GPUContext.
 *
 * @note Skity does not request WGPUAdapter and WGPUDevice, the caller must
 * provide a valid WGPUDevice and WGPUQueue.
 *
 * @param device WGPUDevice A valid WGPUDevice.
 * @param queue WGPUQueue A valid WGPUQueue.
 * @return std::unique_ptr<GPUContext>
 */
std::unique_ptr<GPUContext> SKITY_API WebContextCreate(WGPUDevice device,
                                                       WGPUQueue queue);

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_CONTEXT_WEB_HPP
