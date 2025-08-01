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

  return std::make_shared<GPUTextureMTL>(device.GetMTLDevice(), device.GetMTLCommandQueue(),
                                         texture, descriptor);
}

GPUTextureMTL::GPUTextureMTL(id<MTLDevice> mtl_device, id<MTLCommandQueue> mtl_command_queue,
                             id<MTLTexture> texture, const GPUTextureDescriptor& descriptor)
    : GPUTexture(descriptor),
      mtl_device_(mtl_device),
      mtl_command_queue_(mtl_command_queue),
      mtl_texture_(texture) {}

GPUTextureMTL::~GPUTextureMTL() = default;

size_t GPUTextureMTL::GetBytes() const {
  auto& desc = GetDescriptor();
  if (desc.storage_mode == GPUTextureStorageMode::kMemoryless) {
    return 0;
  }
  return desc.width * desc.height * GetTextureFormatBytesPerPixel(desc.format) * desc.sample_count;
}

}  // namespace skity
