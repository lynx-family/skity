// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/mtl/gpu_blit_pass_mtl.h"

#include "src/gpu/mtl/gpu_buffer_mtl.h"
#include "src/gpu/mtl/gpu_texture_mtl.h"

namespace skity {

GPUBlitPassMTL::GPUBlitPassMTL(id<MTLDevice> mtl_device, id<MTLBlitCommandEncoder> blit_encoder)
    : mtl_device_(mtl_device), blit_encoder_(blit_encoder) {}

void GPUBlitPassMTL::UploadTextureData(std::shared_ptr<GPUTexture> texture, uint32_t offset_x,
                                       uint32_t offset_y, uint32_t width, uint32_t height,
                                       void* data) {
  id<MTLBuffer> upload_buffer =
      [mtl_device_ newBufferWithBytes:data
                               length:width * height *
                                      GetTextureFormatBytesPerPixel(texture->GetDescriptor().format)
                              options:MTLResourceStorageModeShared];
  if (!upload_buffer) return;

  id<MTLTexture> mtl_texture = static_cast<GPUTextureMTL*>(texture.get())->GetMTLTexture();
  [blit_encoder_
           copyFromBuffer:upload_buffer
             sourceOffset:0
        sourceBytesPerRow:width * GetTextureFormatBytesPerPixel(texture->GetDescriptor().format)
      sourceBytesPerImage:width * height *
                          GetTextureFormatBytesPerPixel(texture->GetDescriptor().format)
               sourceSize:MTLSizeMake(width, height, 1)
                toTexture:mtl_texture
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:MTLOriginMake(offset_x, offset_y, 0)];
}

void GPUBlitPassMTL::UploadBufferData(GPUBuffer* buffer, void* data, size_t size) {
  if (size == 0 || data == nullptr) {
    return;
  }

  auto gpu_buffer_metal = static_cast<GPUBufferMTL*>(buffer);
  gpu_buffer_metal->RecreateBufferIfNeeded(size);

  id<MTLBuffer> stage_buffer = [mtl_device_ newBufferWithBytes:data
                                                        length:size
                                                       options:MTLResourceStorageModeShared];
  [blit_encoder_ copyFromBuffer:stage_buffer
                   sourceOffset:0
                       toBuffer:gpu_buffer_metal->GetMTLBuffer()
              destinationOffset:0
                           size:size];
}

void GPUBlitPassMTL::End() { [blit_encoder_ endEncoding]; }

}  // namespace skity
