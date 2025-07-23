// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_TEXTURE_MTL_HPP
#define SRC_GPU_MTL_GPU_TEXTURE_MTL_HPP

#import <Metal/MTLTexture.h>

#include <memory>

#include "src/gpu/backend_cast.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/gpu/mtl/gpu_device_mtl.h"

namespace skity {

class GPUTextureMTL : public GPUTexture {
 public:
  static std::shared_ptr<GPUTextureMTL> Create(
      GPUDeviceMTL& device, const GPUTextureDescriptor& descriptor);

  GPUTextureMTL(id<MTLTexture> texture, const GPUTextureDescriptor& descriptor);

  ~GPUTextureMTL() override;

  id<MTLTexture> GetMTLTexture() const { return mtl_texture_; }

  void UploadData(uint32_t offset_x, uint32_t offset_y, uint32_t width,
                  uint32_t height, void* data) override;

  size_t GetBytes() const override;

  SKT_BACKEND_CAST(GPUTextureMTL, GPUTexture)

 private:
  id<MTLTexture> mtl_texture_;
};

class GPUExternalTextureMTL : public GPUTextureMTL {
 public:
  explicit GPUExternalTextureMTL(const GPUTextureDescriptor& descriptor,
                                 id<MTLTexture> texture,
                                 ReleaseCallback callback,
                                 ReleaseUserData user_data)
      : GPUTextureMTL(texture, descriptor) {
    SetRelease(callback, user_data);
  }

  ~GPUExternalTextureMTL() override = default;

  static std::shared_ptr<GPUTexture> Make(
      const GPUTextureDescriptor& descriptor, id<MTLTexture> texture,
      ReleaseCallback callback = nullptr, ReleaseUserData user_data = nullptr) {
    return std::make_shared<GPUExternalTextureMTL>(descriptor, texture,
                                                   callback, user_data);
  }

  void UploadData(uint32_t offset_x, uint32_t offset_y, uint32_t width,
                  uint32_t height, void* data) override {}
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_TEXTURE_MTL_HPP
