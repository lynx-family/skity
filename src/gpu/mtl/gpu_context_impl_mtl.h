// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_CONTEXT_IMPL_MTL_H
#define SRC_GPU_MTL_GPU_CONTEXT_IMPL_MTL_H

#import <Metal/Metal.h>

#include "src/gpu/gpu_context_impl.hpp"

namespace skity {

class GPUContextImplMTL : public GPUContextImpl {
 public:
  GPUContextImplMTL(id<MTLDevice> device, id<MTLCommandQueue> queue);

  ~GPUContextImplMTL() override = default;

  std::unique_ptr<GPUSurface> CreateSurface(
      GPUSurfaceDescriptor *desc) override;

  std::unique_ptr<GPUSurface> CreateFxaaSurface(
      GPUSurfaceDescriptor *desc) override;

  id<MTLCommandQueue> GetNativeQueue() const { return queue_; }

  id<MTLDevice> GetNativeDevice() const { return device_; }

 protected:
  std::unique_ptr<GPUDevice> CreateGPUDevice() override;

  std::shared_ptr<GPUTexture> OnWrapTexture(GPUBackendTextureInfo *info,
                                            ReleaseCallback callback,
                                            ReleaseUserData user_data) override;

  std::unique_ptr<GPURenderTarget> OnCreateRenderTarget(
      const GPURenderTargetDescriptor &desc,
      std::shared_ptr<Texture> texture) override;

  std::shared_ptr<Data> OnReadPixels(
      const std::shared_ptr<GPUTexture> &texture) const override;

 private:
  id<MTLDevice> device_;
  id<MTLCommandQueue> queue_;
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_CONTEXT_IMPL_MTL_H
