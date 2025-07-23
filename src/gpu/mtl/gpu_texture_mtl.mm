// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/mtl/gpu_texture_mtl.h"

#include <memory>
#include "src/gpu/gpu_texture.hpp"
#include "src/gpu/mtl/formats_mtl.h"

namespace skity {

std::shared_ptr<GPUTextureMTL> GPUTextureMTL::Create(GPUDeviceMTL& device,
                                                     const GPUTextureDescriptor& descriptor) {
  id<MTLTexture> texture = [device.GetMTLDevice()
      newTextureWithDescriptor:ToMTLTextureDescriptor(descriptor, device.IsSupportsMemoryless())];

  if (!texture) {
    return nullptr;
  }

  return std::make_shared<GPUTextureMTL>(texture, descriptor);
}

GPUTextureMTL::GPUTextureMTL(id<MTLTexture> texture, const GPUTextureDescriptor& descriptor)
    : GPUTexture(descriptor), mtl_texture_(texture) {}

GPUTextureMTL::~GPUTextureMTL() = default;

void GPUTextureMTL::UploadData(uint32_t offset_x, uint32_t offset_y, uint32_t width,
                               uint32_t height, void* data) {
  [mtl_texture_ replaceRegion:MTLRegionMake2D(offset_x, offset_y, width, height)
                  mipmapLevel:0
                    withBytes:data
                  bytesPerRow:width * GetTextureFormatBytesPerPixel(GetDescriptor().format)];
}

size_t GPUTextureMTL::GetBytes() const {
  auto& desc = GetDescriptor();
  if (desc.storage_mode == GPUTextureStorageMode::kMemoryless) {
    return 0;
  }
  return desc.width * desc.height * GetTextureFormatBytesPerPixel(desc.format) * desc.sample_count;
}

}  // namespace skity
