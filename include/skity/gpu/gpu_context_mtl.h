// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GPU_GPU_CONTEXT_MTL_H
#define INCLUDE_SKITY_GPU_GPU_CONTEXT_MTL_H

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_surface.hpp>
#include <skity/gpu/texture.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * @enum MTLSurfaceType indicate which type the Metal backend Surface targets on
 *
 */
enum class MTLSurfaceType {
  /**
   * empty type, default value
   */
  kInvalid,
  /**
   * Indicate the Surface is target on a  MTLTexture
   *
   * @note The texture must has `MTLTextureUsageRenderTarget` flag in it's
   *       MTLTextureDescriptor
   */
  kTexture,
  /**
   * Indicate the Surface is target on a CAMetalLayer,
   *
   * @note The user needs to handle CALayer size change, and sync the size to
   * drawable
   */
  kLayer,
};

struct GPUSurfaceDescriptorMTL : public GPUSurfaceDescriptor {
  MTLSurfaceType surface_type = MTLSurfaceType::kInvalid;
  CAMetalLayer* layer = nil;
  id<MTLTexture> texture = nil;
};

struct GPUBackendTextureInfoMTL : public GPUBackendTextureInfo {
  id<MTLTexture> texture = nil;
};

/**
 *  Create a GPUContext instance target on Metal backend
 *
 * @param device current picked MTLDevice, pass nil can let Skity engine pick
 *               the device
 * @param queue  current create CommandQueue, pass nil can let Skity engine
 *               create it's own CommandQueue
 * @return       GPUContext instance, or null if creation failed
 */
std::unique_ptr<GPUContext> SKITY_API
MTLContextCreate(id<MTLDevice> device, id<MTLCommandQueue> queue);

}  // namespace skity

#endif  // INCLUDE_SKITY_GPU_GPU_CONTEXT_MTL_H
