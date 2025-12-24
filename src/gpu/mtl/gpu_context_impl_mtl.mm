// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "src/gpu/mtl/gpu_context_impl_mtl.h"

#include <skity/gpu/gpu_context_mtl.h>

#include "src/gpu/mtl/formats_mtl.h"
#include "src/gpu/mtl/gpu_device_mtl.h"
#include "src/gpu/mtl/gpu_surface_mtl.h"
#include "src/gpu/mtl/gpu_texture_mtl.h"

namespace skity {

std::unique_ptr<GPUContext> MTLContextCreate(id<MTLDevice> device, id<MTLCommandQueue> queue) {
  if (device == nil) {
    device = MTLCreateSystemDefaultDevice();
  }

  if (queue == nil) {
    queue = [device newCommandQueue];
  }

  auto ctx = std::make_unique<GPUContextImplMTL>(device, queue);

  ctx->Init();

  return ctx;
}

id<MTLDevice> MTLContextGetDevice(GPUContext *context) {
  if (context == nullptr) {
    return nil;
  }

  if (context->GetBackendType() != GPUBackendType::kMetal) {
    return nil;
  }

  auto mtl_context = static_cast<GPUContextImplMTL *>(context);

  return mtl_context->GetNativeDevice();
}

id<MTLCommandQueue> MTLContextGetCommandQueue(GPUContext *context) {
  if (context == nullptr) {
    return nil;
  }

  if (context->GetBackendType() != GPUBackendType::kMetal) {
    return nil;
  }

  auto mtl_context = static_cast<GPUContextImplMTL *>(context);

  return mtl_context->GetNativeQueue();
}

GPUContextImplMTL::GPUContextImplMTL(id<MTLDevice> device, id<MTLCommandQueue> queue)
    : GPUContextImpl(GPUBackendType::kMetal), device_(device), queue_(queue) {}

std::unique_ptr<GPUSurface> GPUContextImplMTL::CreateSurface(GPUSurfaceDescriptor *desc) {
  if (desc->backend != GPUBackendType::kMetal) {
    return {};
  }

  auto mtl_desc = static_cast<GPUSurfaceDescriptorMTL *>(desc);

  if (mtl_desc->surface_type == MTLSurfaceType::kLayer) {
    if (mtl_desc->layer == nil) {
      return {};
    }

    return std::make_unique<MTLLayerSurface>(*desc, this, mtl_desc->layer);
  } else if (mtl_desc->surface_type == MTLSurfaceType::kTexture) {
    if (mtl_desc->texture == nil) {
      return {};
    }

    return std::make_unique<MTLTextureSurface>(*desc, this, mtl_desc->texture);
  }

  return {};
}

std::unique_ptr<GPUSurface> GPUContextImplMTL::CreateFxaaSurface(GPUSurfaceDescriptor *desc) {
  return nullptr;
}

std::unique_ptr<GPURenderTarget> GPUContextImplMTL::OnCreateRenderTarget(
    const GPURenderTargetDescriptor &desc, std::shared_ptr<Texture> texture) {
  if (texture == nullptr) {
    return {};
  }

  auto mtl_texture = static_cast<GPUTextureMTL *>(texture->GetGPUTexture().get());

  GPUSurfaceDescriptorMTL surface_desc{};
  surface_desc.backend = GetBackendType();
  surface_desc.width = desc.width;
  surface_desc.height = desc.height;
  surface_desc.content_scale = 1.0;
  surface_desc.sample_count = desc.sample_count;
  surface_desc.surface_type = MTLSurfaceType::kTexture;
  surface_desc.texture = mtl_texture->GetMTLTexture();

  auto surface = CreateSurface(&surface_desc);

  return std::make_unique<GPURenderTarget>(std::move(surface), texture);
}

std::unique_ptr<GPUDevice> GPUContextImplMTL::CreateGPUDevice() {
  return std::make_unique<GPUDeviceMTL>(device_, queue_);
}

std::shared_ptr<GPUTexture> GPUContextImplMTL::OnWrapTexture(GPUBackendTextureInfo *info,
                                                             ReleaseCallback callback,
                                                             ReleaseUserData user_data) {
  if (info->backend != GPUBackendType::kMetal) {
    return {};
  }

  auto mtl_info = static_cast<GPUBackendTextureInfoMTL *>(info);

  if (mtl_info->texture == nil) {
    return {};
  }

  GPUTextureDescriptor descriptor;
  descriptor.width = mtl_info->texture.width;
  descriptor.height = mtl_info->texture.height;
  descriptor.format = ToGPUTextureFormat(mtl_info->texture.pixelFormat);
  descriptor.usage = static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
  descriptor.storage_mode = GPUTextureStorageMode::kHostVisible;

  return GPUExternalTextureMTL::Make(descriptor, mtl_info->texture, callback, user_data);
}

std::shared_ptr<Data> GPUContextImplMTL::OnReadPixels(
    const std::shared_ptr<GPUTexture> &texture) const {
  auto desc = texture->GetDescriptor();
  auto storage_mode = desc.storage_mode;
  id<MTLTexture> mtl_texture = static_cast<GPUTextureMTL *>(texture.get())->GetMTLTexture();
  if (storage_mode == GPUTextureStorageMode::kMemoryless) {
    return nullptr;
  }

  id<MTLBuffer> buffer = [device_ newBufferWithLength:texture->GetBytes()
                                              options:MTLResourceStorageModeShared];
  id<MTLCommandBuffer> command_buffer = [queue_ commandBuffer];
  id<MTLBlitCommandEncoder> blit_command_encoder = [command_buffer blitCommandEncoder];
  auto dst_bytes_per_row = desc.width * GetTextureFormatBytesPerPixel(desc.format);
  auto dst_bytes_per_image = desc.height * dst_bytes_per_row;
  [blit_command_encoder copyFromTexture:mtl_texture
                            sourceSlice:0
                            sourceLevel:0
                           sourceOrigin:MTLOriginMake(0, 0, 0)
                             sourceSize:MTLSizeMake(desc.width, desc.height, 1)
                               toBuffer:buffer
                      destinationOffset:0
                 destinationBytesPerRow:dst_bytes_per_row
               destinationBytesPerImage:dst_bytes_per_image];
  [blit_command_encoder endEncoding];
  [command_buffer commit];
  [command_buffer waitUntilCompleted];
  return Data::MakeWithCopy(buffer.contents, buffer.length);
}

}  // namespace skity
